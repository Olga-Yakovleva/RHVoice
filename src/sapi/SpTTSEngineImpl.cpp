/* Copyright (C) 2010, 2011  Olga Yakovleva <yakovleva.o.v@gmail.com> */

/* This program is free software: you can redistribute it and/or modify */
/* it under the terms of the GNU General Public License as published by */
/* the Free Software Foundation, either version 3 of the License, or */
/* (at your option) any later version. */

/* This program is distributed in the hope that it will be useful, */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
/* GNU General Public License for more details. */

/* You should have received a copy of the GNU General Public License */
/* along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include <new>
#include <locale>
#include <algorithm>
#include "SpTTSEngineImpl.h"
#include "global.h"
#include "win32_exception.h"
#include "util.h"

using std::bad_alloc;
using std::string;
using std::wstring;
using std::wistringstream;
using std::wostringstream;
using std::locale;
using std::map;
using std::min;
using std::max;
using std::count;

map<size_t,const SPVTEXTFRAG*> CSpTTSEngineImpl::frag_map=map<size_t,const SPVTEXTFRAG*>();
ISpTTSEngineSite *CSpTTSEngineImpl::out=NULL;
wstring CSpTTSEngineImpl::ssml=wstring();
float CSpTTSEngineImpl::pitch_table[21]={0.749154,
                                         0.771105,
                                         0.793701,
                                         0.816958,
                                         0.840896,
                                         0.865537,
                                         0.890899,
                                         0.917004,
                                         0.943874,
                                         0.971532,
                                         1.000000,
                                         1.029302,
                                         1.059463,
                                         1.090508,
                                         1.122462,
                                         1.155353,
                                         1.189207,
                                         1.224054,
                                         1.259921,
                                         1.296840,
                                         1.334840};
float CSpTTSEngineImpl::rate_table[21]={0.333333,
                                        0.372041,
                                        0.415244,
                                        0.463463,
                                        0.517282,
                                        0.577350,
                                        0.644394,
                                        0.719223,
                                        0.802742,
                                        0.895958,
                                        1.000000,
                                        1.116123,
                                        1.245731,
                                        1.390389,
                                        1.551846,
                                        1.732051,
                                        1.933182,
                                        2.157669,
                                        2.408225,
                                        2.687875,
                                        3.000000};
unsigned long long CSpTTSEngineImpl::audio_bytes=0;

CSpTTSEngineImpl::CSpTTSEngineImpl()
{
  ref_count=0;
  InterlockedIncrement(&svr_ref_count);
  sample_rate=0;
}

CSpTTSEngineImpl::~CSpTTSEngineImpl()
{
  InterlockedDecrement(&svr_ref_count);
  if(sample_rate!=0) RHVoice_terminate();
}

STDMETHODIMP_(ULONG) CSpTTSEngineImpl::AddRef()
{
  return ++ref_count;
}

STDMETHODIMP_(ULONG) CSpTTSEngineImpl::Release()
{
  if(--ref_count==0)
    {
      delete this;
      return 0;
    }
  return ref_count;
}

STDMETHODIMP CSpTTSEngineImpl::QueryInterface(REFIID riid,void** ppv)
{
  HRESULT result=S_OK;
  try
    {
      *ppv=NULL;
      if(IsEqualIID(riid,IID_IUnknown))
        *ppv=static_cast<IUnknown*>(static_cast<ISpTTSEngine*>(this));
      else if(IsEqualIID(riid,IID_ISpTTSEngine))
        *ppv=static_cast<ISpTTSEngine*>(this);
      else if(IsEqualIID(riid,IID_ISpObjectWithToken))
        *ppv=static_cast<ISpObjectWithToken*>(this);
      else
        result=E_NOINTERFACE;
      if(result==S_OK)
        reinterpret_cast<IUnknown*>(*ppv)->AddRef();
    }
  catch(win32_exception& e)
    {
      result=(e.code==EXCEPTION_ACCESS_VIOLATION)?E_POINTER:E_UNEXPECTED;
    }
  return result;
}

STDMETHODIMP CSpTTSEngineImpl::Speak(DWORD dwSpeakFlags,REFGUID rguidFormatId,const WAVEFORMATEX *pWaveFormatEx,const SPVTEXTFRAG *pTextFragList,ISpTTSEngineSite *pOutputSite)
{
  HRESULT result=S_OK;
  try
    {
      if((pTextFragList==NULL)||(pOutputSite==NULL))
        return E_INVALIDARG;
      if(sample_rate==0)
        return E_UNEXPECTED;
      out=pOutputSite;
      set_rate();
      set_volume();
      RHVoice_set_pitch(50);
      generate_ssml(pTextFragList);
      RHVoice_message msg=RHVoice_new_message_utf16(reinterpret_cast<const uint16_t*>(ssml.data()),ssml.size(),1);
      if(msg==NULL) return E_FAIL;
      audio_bytes=0;
      try
        {
          RHVoice_speak(msg);
        }
      catch(...)
        {
          RHVoice_delete_message(msg);
          throw;
        }
      RHVoice_delete_message(msg);
    }
  catch(bad_alloc&)
    {
      result=E_OUTOFMEMORY;
    }
  catch(win32_exception& e)
    {
      result=(e.code==EXCEPTION_ACCESS_VIOLATION)?E_POINTER:E_UNEXPECTED;
    }
  catch(...)
    {
      result=E_UNEXPECTED;
    }
  return result;
}

STDMETHODIMP CSpTTSEngineImpl::GetOutputFormat(const GUID *pTargetFmtId,const WAVEFORMATEX *pTargetWaveFormatEx,GUID *pOutputFormatId,WAVEFORMATEX **ppCoMemOutputWaveFormatEx)
{
  HRESULT result=S_OK;
  try
    {
      if((pOutputFormatId==NULL)||(ppCoMemOutputWaveFormatEx==NULL))
        return E_INVALIDARG;
      if(sample_rate==0)
        return E_UNEXPECTED;
      *pOutputFormatId=SPDFID_WaveFormatEx;
      *ppCoMemOutputWaveFormatEx=NULL;
      WAVEFORMATEX *pwfex=reinterpret_cast<WAVEFORMATEX*>(CoTaskMemAlloc(sizeof(WAVEFORMATEX)));
      if(pwfex==NULL)
        return E_OUTOFMEMORY;
      pwfex->wFormatTag=0x0001;
      pwfex->nChannels=1;
      pwfex->nSamplesPerSec=sample_rate;
      pwfex->wBitsPerSample=16;
      pwfex->nBlockAlign=pwfex->nChannels*pwfex->wBitsPerSample/8;
      pwfex->nAvgBytesPerSec=pwfex->nSamplesPerSec*pwfex->nBlockAlign;
      pwfex->cbSize=0;
      *ppCoMemOutputWaveFormatEx=pwfex;
    }
  catch(win32_exception& e)
    {
      result=(e.code==EXCEPTION_ACCESS_VIOLATION)?E_POINTER:E_UNEXPECTED;
    }
  catch(...)
    {
      result=E_UNEXPECTED;
    }
  return result;
}

STDMETHODIMP CSpTTSEngineImpl::SetObjectToken(ISpObjectToken *pToken)
{
  HRESULT result=S_OK;
  try
    {
      if(pToken==NULL)
        return E_INVALIDARG;
      object_token.Attach(pToken,true);
      wchar_t *voice_path_w=NULL;
      if(FAILED(pToken->GetStringValue(L"VoicePath",&voice_path_w)))
        return E_FAIL;
      string voice_path;
      try
        {
          voice_path=wstring_to_string(voice_path_w);
        }
      catch(...)
        {
          CoTaskMemFree(voice_path_w);
          throw;
        }
      CoTaskMemFree(voice_path_w);
      voice_path_w=NULL;
      if(sample_rate!=0) RHVoice_terminate();
      sample_rate=RHVoice_initialize(voice_path.c_str(),callback);
      if(sample_rate==0)
        return E_FAIL;
    }
  catch(bad_alloc&)
    {
      result=E_OUTOFMEMORY;
    }
  catch(win32_exception& e)
    {
      result=(e.code==EXCEPTION_ACCESS_VIOLATION)?E_POINTER:E_UNEXPECTED;
    }
  catch(...)
    {
      result=E_UNEXPECTED;
    }
  return result;
}

STDMETHODIMP CSpTTSEngineImpl::GetObjectToken(ISpObjectToken **ppToken)
{
  HRESULT result=S_OK;
  try
    {
      *ppToken=NULL;
      if(!object_token)
        return E_FAIL;
      *ppToken=object_token.GetInterfacePtr();
      (*ppToken)->AddRef();
    }
  catch(win32_exception& e)
    {
      result=(e.code==EXCEPTION_ACCESS_VIOLATION)?E_POINTER:E_UNEXPECTED;
    }
  catch(...)
    {
      result=E_UNEXPECTED;
    }
  return result;
}

float CSpTTSEngineImpl::get_pitch_factor(int value)
{
  return pitch_table[max(-10,min(10,value))+10];
}

float CSpTTSEngineImpl::get_rate_factor(int value)
{
  return rate_table[max(-10,min(10,value))+10];
}

void CSpTTSEngineImpl::set_rate()
{
  long rate;
  out->GetRate(&rate);
  RHVoice_set_rate(20.0*get_rate_factor(rate));
}

float CSpTTSEngineImpl::convert_volume(unsigned int value)
{
  return value>100?100:value;
}

void CSpTTSEngineImpl::set_volume()
{
  unsigned short volume;
  out->GetVolume(&volume);
  RHVoice_set_volume(convert_volume(volume));
}

void CSpTTSEngineImpl::write_text_to_stream(wostringstream& s,const wchar_t *text,size_t len)
{
  for(size_t i=0;i<len;i++)
    {
      if(text[i]==L'<')
        s << L"&#60;";
      else if(text[i]==L'&')
        s << L"&#38;";
      else if(text[i]==L'"')
        s << L"&#34;";
      else
        s << text[i];
    }
}

void CSpTTSEngineImpl::generate_ssml(const SPVTEXTFRAG *frags)
{
  frag_map.clear();
  wostringstream s;
  s.exceptions(wostringstream::failbit|wostringstream::badbit);
  s << L"<speak>";
  for(const SPVTEXTFRAG *frag=frags;frag;frag=frag->pNext)
    {
      switch(frag->State.eAction)
        {
        case SPVA_Speak:
        case SPVA_Pronounce:
        case SPVA_SpellOut:
          if(frag->ulTextLen>0)
            {
              s << L"<prosody rate=\"";
              s << 100.0*get_rate_factor(frag->State.RateAdj);
              s << L"%\" pitch=\"";
              s << 100.0*get_pitch_factor(frag->State.PitchAdj.MiddleAdj);
              s << L"%\" volume=\"";
              s << convert_volume(frag->State.Volume);
              s << L"%\">";
              frag_map[s.tellp()]=frag;
              write_text_to_stream(s,frag->pTextStart,frag->ulTextLen);
              s << L"</prosody>";
            }
          break;
        case SPVA_Silence:
          s << L"<break time=\"";
          s << frag->State.SilenceMSecs;
          s << L"ms\"/>";
          break;
        case SPVA_Bookmark:
          s << L"<mark name=\"";
          write_text_to_stream(s,frag->pTextStart,frag->ulTextLen);
          s << L"\"/>";
          break;
        default:
          break;
        }
    }
  s << L"</speak>";
  ssml.assign(s.str());
}

wstring::const_iterator CSpTTSEngineImpl::skip_unichars(wstring::const_iterator first,size_t n)
{
  size_t i;
  wstring::const_iterator it;
  for(i=0,it=first;(it!=ssml.end())&&(i<n);it=skip_unichar(it),i++);
  return it;
}

int CSpTTSEngineImpl::callback(const short *samples,int num_samples,const RHVoice_event *events,int num_events)
{
  int result=1;
  try
    {
      DWORD a=out->GetActions();
      if(a&SPVES_ABORT) return 0;
      if(a&SPVES_SKIP) return 0;
      if(a&SPVES_RATE) set_rate();
      if(a&SPVES_VOLUME) set_volume();
      SPEVENT e;
      e.ulStreamNum=0;
      unsigned long bytes_written=0;
      map<size_t,const SPVTEXTFRAG*>::const_iterator frag_pos;
      wstring::const_iterator frag_start,start,end;
      long n;
      wstring m;
      wistringstream s;
      s.exceptions(wistringstream::failbit|wistringstream::badbit|wistringstream::eofbit);
      for(int i=0;i<num_events;i++)
        {
          if(events[i].type==RHVoice_event_mark)
            {
              e.eEventId=SPEI_TTS_BOOKMARK;
              e.elParamType=SPET_LPARAM_IS_STRING;
            }
          else if(events[i].type==RHVoice_event_word_start)
            {
              e.eEventId=SPEI_WORD_BOUNDARY;
              e.elParamType=SPET_LPARAM_IS_UNDEFINED;
            }
          else if(events[i].type==RHVoice_event_sentence_start)
            {
              e.eEventId=SPEI_SENTENCE_BOUNDARY;
              e.elParamType=SPET_LPARAM_IS_UNDEFINED;
            }
          else continue;
          e.ullAudioStreamOffset=audio_bytes+events[i].audio_position*sizeof(short);
          if(events[i].type==RHVoice_event_mark)
            {
              m.assign(string_to_wstring(events[i].id.name));
              e.lParam=(LPARAM)m.c_str();
              s.str(m);
              try
                {
                  s >> n;
                }
              catch(...)
                {
                  n=0;
                }
              e.wParam=n;
            }
          else
            {
              start=skip_unichars(ssml.begin(),events[i].text_position-1);
              end=skip_unichars(start,events[i].text_length);
              e.wParam=end-start-count(start,end,L'&')*4;
              frag_pos=frag_map.lower_bound(start-ssml.begin());
              if((frag_pos==frag_map.end())||
                 (frag_pos->first>(start-ssml.begin())))
                frag_pos--;
              frag_start=ssml.begin()+frag_pos->first;
              e.lParam=frag_pos->second->ulTextSrcOffset+(start-frag_start-count(frag_start,start,L'&'));
            }
          out->AddEvents(&e,1);
        }
      if(FAILED(out->Write(samples,num_samples*sizeof(short),&bytes_written)))
        result=0;
      audio_bytes+=bytes_written;
    }
  catch(...)
    {
      result=0;
    }
  return result;
}