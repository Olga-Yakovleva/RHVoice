/* Copyright (C) 2017  Olga Yakovleva <yakovleva.o.v@gmail.com> */

/* This program is free software: you can redistribute it and/or modify */
/* it under the terms of the GNU Lesser General Public License as published by */
/* the Free Software Foundation, either version 3 of the License, or */
/* (at your option) any later version. */

/* This program is distributed in the hope that it will be useful, */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
/* GNU Lesser General Public License for more details. */

/* You should have received a copy of the GNU Lesser General Public License */
/* along with this program.  If not, see <http://www.gnu.org/licenses/>. */

package com.github.olga_yakovleva.rhvoice.android;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;

public final class MainActivity extends Activity implements AvailableLanguagesFragment.Listener,AvailableVoicesFragment.Listener,ConfirmVoiceRemovalDialogFragment.Listener
{
    @Override
    protected void onCreate(Bundle state)
    {
        super.onCreate(state);
        setContentView(R.layout.frame);
        if(state==null)
            getFragmentManager().beginTransaction().replace(R.id.frame,new AvailableLanguagesFragment(),"languages").commit();
}

    public void onLanguageSelected(LanguagePack language)
    {
        Bundle args=new Bundle();
        args.putString(AvailableVoicesFragment.ARG_LANGUAGE,language.getCode());
        AvailableVoicesFragment frag=new AvailableVoicesFragment();
        frag.setArguments(args);
        getFragmentManager().beginTransaction().replace(R.id.frame,frag,"voices").addToBackStack(null).commit();
}

    public void onVoiceSelected(VoicePack voice,boolean state)
    {
        if(state||!voice.isInstalled(this))
            {
                voice.setEnabled(this,state);
                startService(new Intent(this,DataService.class));
                AvailableVoicesFragment frag=(AvailableVoicesFragment)(getFragmentManager().findFragmentByTag("voices"));
                if(frag!=null)
                    frag.refresh();
            }
        else
            {
                ConfirmVoiceRemovalDialogFragment.show(this,voice);
            }
    }

    public void onConfirmVoiceRemovalResponse(VoicePack voice,boolean response)
    {
        if(response)
            {
                voice.setEnabled(this,false);
                startService(new Intent(this,DataService.class));
                AvailableVoicesFragment frag=(AvailableVoicesFragment)(getFragmentManager().findFragmentByTag("voices"));
                if(frag!=null)
                    frag.refresh();
            }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu)
    {
        getMenuInflater().inflate(R.menu.main,menu);
        return super.onCreateOptionsMenu(menu);
}

    private void showSettings()
    {
        Intent intent=new Intent(this,SettingsActivity.class);
        startActivity(intent);
}

    @Override
    public boolean onOptionsItemSelected(MenuItem item)
    {
        switch(item.getItemId())
            {
            case R.id.settings:
                showSettings();
                return true;
            default:
                return super.onOptionsItemSelected(item);
}
}
}