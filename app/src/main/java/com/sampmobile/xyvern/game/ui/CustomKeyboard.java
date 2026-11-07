package com.sampmobile.xyvern.game.ui;

import android.app.Activity;
import android.widget.Button;
import android.widget.EditText;

import androidx.constraintlayout.widget.ConstraintLayout;

import com.joom.paranoid.Obfuscate;
import com.sampmobile.xyvern.R;

import android.app.Activity;
import android.content.Context;
import android.text.Editable;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.sampmobile.xyvern.R;

import java.util.ArrayList;
@Obfuscate
public class CustomKeyboard {

    public interface InputListener
    {
        public void OnInputEnd(String str);
    }

    private LinearLayout mInputLayout = null;
    private EditText mInputEt = null;

    private Button mButtonSlash = null;
    private Button mButtonHistoryNext = null;
    private Button mButtonHistoryPrev = null;

    private Activity mContext = null;
    private boolean mIsShowing = false;

    private String mSavedInput = null;

    private final InputHistory mInputHistory = new InputHistory(20);

    private Runnable mAnimTask = null;

    public boolean IsShowing()
    {
        return mIsShowing;
    }

    public CustomKeyboard(Activity act)
    {
        mContext = act;

        ConstraintLayout layout = (ConstraintLayout) act.getLayoutInflater().inflate(R.layout.layout_keyboard, null);
        mContext.addContentView(layout, new ConstraintLayout.LayoutParams(-1, -1));

        mInputLayout = layout.findViewById(R.id.main_input);
        mInputEt = layout.findViewById(R.id.main_et_input);
        mButtonSlash = layout.findViewById(R.id.main_but_input_slash);
        mButtonHistoryPrev = layout.findViewById(R.id.main_but_input_prev);
        mButtonHistoryNext = layout.findViewById(R.id.main_but_input_next);


        mButtonSlash.setOnClickListener(view -> {
            int cursor = Math.max(0, mInputEt.getSelectionStart());
            mInputEt.getText().insert(cursor, "/");
        });
        mButtonHistoryPrev.setOnClickListener(view -> showHistory(
                mInputHistory.previous(mInputEt.getText().toString())));
        mButtonHistoryNext.setOnClickListener(view -> showHistory(
                mInputHistory.next(mInputEt.getText().toString())));

        mInputEt.setOnEditorActionListener((textView, action, event) -> {
            boolean enter = event != null && event.getKeyCode() == KeyEvent.KEYCODE_ENTER
                    && event.getAction() == KeyEvent.ACTION_DOWN;
            if (enter || action == EditorInfo.IME_ACTION_DONE || action == EditorInfo.IME_ACTION_NEXT
                    || action == EditorInfo.IME_ACTION_SEND || action == EditorInfo.IME_ACTION_GO) {
                String input = mInputEt.getText().toString();
                mInputEt.setText("");
                OnInputEnd(input);
                return true;
            }
            return false;
        });

        HideInputLayout();
    }

    public void onHeightChanged(int height)
    {
        android.view.ViewGroup.MarginLayoutParams params = (android.view.ViewGroup.MarginLayoutParams) mInputLayout.getLayoutParams();
        params.setMargins(0, 0 , 0, height);
        mInputLayout.setLayoutParams(params);
    }

    public void ShowInputLayout()
    {
        mIsShowing = true;

        mInputLayout.setVisibility(View.INVISIBLE);

        mInputEt.requestFocus();
        InputMethodManager imm = (InputMethodManager) mContext.getSystemService(Context.INPUT_METHOD_SERVICE);
        imm.showSoftInput(mInputEt, InputMethodManager.SHOW_IMPLICIT);

        if(mAnimTask != null)
        {
            mInputLayout.removeCallbacks(mAnimTask);
            mAnimTask = null;
        }

        mAnimTask = new Runnable() {
            @Override
            public void run() {
                mInputLayout.setVisibility(View.VISIBLE);
                mInputEt.requestFocus();
            }
        };

        mInputLayout.postDelayed(mAnimTask, 60);

        mInputHistory.reset();

        if(mSavedInput != null)
        {
            mInputEt.setText(mSavedInput);
            mInputEt.setSelection(mInputEt.getText().length());
        }
    }

    public void HideInputLayout()
    {

        mInputHistory.reset();

        if(mInputEt.getEditableText() != null)
        {
            mSavedInput = mInputEt.getEditableText().toString();
        }

        if(mAnimTask != null)
        {
            mInputLayout.removeCallbacks(mAnimTask);
            mAnimTask = null;
        }

        if(mContext.getCurrentFocus() != null)
        {
            InputMethodManager inputMethodManager = (InputMethodManager) mContext.getSystemService(Context.INPUT_METHOD_SERVICE);
            inputMethodManager.hideSoftInputFromWindow(mContext.getCurrentFocus().getWindowToken(), 0);
        }
        mInputLayout.setVisibility(View.GONE);

        mIsShowing = false;
    }

    private void showHistory(String text) {
        mInputEt.setText(text);
        mInputEt.setSelection(mInputEt.getText().length());
    }

    private void OnInputEnd(String str)
    {
        mInputHistory.add(str);
        InputListener listener = (InputListener)mContext;
        listener.OnInputEnd(str);
    }
}
