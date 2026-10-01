package com.sampmobile.xyvern.launcher.util;

import android.annotation.SuppressLint;
import android.content.Context;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.widget.EditText;

import com.nvidia.devtech.NvEventQueueActivity;
import com.sampmobile.xyvern.game.SAMP;

@SuppressLint("AppCompatCustomView")
public class CustomEditText extends EditText {
    private Context mContext = null;

    public CustomEditText(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    public void SetBackListener(Context context) {
        mContext = context;
    }

    @Override
    public boolean onKeyPreIme(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            ((SAMP)mContext).onEventBackPressed();
            return true;
        }
        return false;
    }
}
