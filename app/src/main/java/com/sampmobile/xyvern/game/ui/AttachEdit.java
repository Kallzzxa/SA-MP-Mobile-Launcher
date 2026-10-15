package com.sampmobile.xyvern.game.ui;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import androidx.constraintlayout.widget.ConstraintLayout;

import com.joom.paranoid.Obfuscate;
import com.sampmobile.xyvern.R;
import com.sampmobile.xyvern.launcher.util.Util;
@Obfuscate
public class AttachEdit {
    // Native button types (must match C++ AttachClick handler)
    private final int POSITION_Y = 0;
    private final int POSITION_Z = 1;
    private final int POSITION_X = 2;
    private final int BUTTON_SCALE = 3;
    private final int BUTTON_ROT_X = 4;
    private final int BUTTON_ROT_Y = 5;
    private final int BUTTON_ROT_Z = 6;

    // UI modes (PC-style edit object)
    private final int MODE_POS = 0;
    private final int MODE_ROT = 1;
    private final int MODE_SCALE = 2;

    private int active_mode = -1;
    private int pos_axis = 0; // 0=X, 1=Y, 2=Z
    private int rot_axis = 0; // 0=X, 1=Y, 2=Z

    private final int[] POS_BUTTONS = {POSITION_X, POSITION_Y, POSITION_Z};
    private final int[] ROT_BUTTONS = {BUTTON_ROT_X, BUTTON_ROT_Y, BUTTON_ROT_Z};
    private final String[] POS_NAMES = {"X", "Y", "Z"};
    private final String[] POS_DESC = {
        "Atur posisi X (geser kiri/kanan)",
        "Atur posisi Y (geser depan/belakang)",
        "Atur posisi Z (naik/turun)"
    };
    private final String[] ROT_DESC = {
        "Atur rotasi X",
        "Atur rotasi Y",
        "Atur rotasi Z"
    };

    private Activity activity;
    private ConstraintLayout attach_main_layot;
    private Button attach_mode_pos;
    private Button attach_mode_rot;
    private Button attach_mode_scale;
    private Button attach_mode_save;
    private TextView attach_text_description;
    private Button attach_left_butt;
    private Button attach_right_button;

    native void Exit();
    native void AttachClick(int buttonType, boolean buttonID);
    native void Save();
    native void SetGizmoAxis(int axis);

    public boolean isShow;

    @SuppressLint("ClickableViewAccessibility")
    public AttachEdit(Activity activity)
    {
        this.activity = activity;

        ConstraintLayout layout = (ConstraintLayout) activity.getLayoutInflater().inflate(R.layout.attachedit, null);
        activity.addContentView(layout, new ConstraintLayout.LayoutParams(-1, -1));

        attach_mode_pos = activity.findViewById(R.id.attach_mode_pos);
        attach_mode_rot = activity.findViewById(R.id.attach_mode_rot);
        attach_mode_scale = activity.findViewById(R.id.attach_mode_scale);
        attach_mode_save = activity.findViewById(R.id.attach_mode_save);
        attach_text_description = activity.findViewById(R.id.attach_text_description);
        attach_left_butt = activity.findViewById(R.id.attach_left_butt);
        attach_right_button = activity.findViewById(R.id.attach_right_button);

        attach_left_butt.setOnTouchListener(touchListener);
        attach_right_button.setOnTouchListener(touchListener);

        attach_mode_pos.setOnClickListener(view -> {
            if (active_mode == MODE_POS) {
                // Cycle axis X -> Y -> Z
                pos_axis = (pos_axis + 1) % 3;
            } else {
                active_mode = MODE_POS;
            }
            updateModeUI();
        });

        attach_mode_rot.setOnClickListener(view -> {
            if (active_mode == MODE_ROT) {
                // Cycle axis X -> Y -> Z
                rot_axis = (rot_axis + 1) % 3;
            } else {
                active_mode = MODE_ROT;
            }
            updateModeUI();
        });

        attach_mode_scale.setOnClickListener(view -> {
            active_mode = MODE_SCALE;
            updateModeUI();
        });

        attach_mode_save.setOnClickListener(view -> {
            Util.HideLayout(attach_main_layot, true);
            isShow = false;
            Save();
        });

        attach_main_layot = activity.findViewById(R.id.attach_main_layot);

        isShow = false;
        Util.HideLayout(attach_main_layot, true);
    }

    private int getActiveButtonType() {
        if (active_mode == MODE_POS) return POS_BUTTONS[pos_axis];
        if (active_mode == MODE_ROT) return ROT_BUTTONS[rot_axis];
        if (active_mode == MODE_SCALE) return BUTTON_SCALE;
        return -1;
    }

    private void updateModeUI() {
        int red = Color.parseColor("#C62828");
        int dark = Color.parseColor("#212121");

        attach_mode_pos.setBackgroundTintList(ColorStateList.valueOf(active_mode == MODE_POS ? red : dark));
        attach_mode_rot.setBackgroundTintList(ColorStateList.valueOf(active_mode == MODE_ROT ? red : dark));
        attach_mode_scale.setBackgroundTintList(ColorStateList.valueOf(active_mode == MODE_SCALE ? red : dark));

        int gizmoAxis = -1;
        if (active_mode == MODE_POS) {
            attach_text_description.setText("Position mode — " + POS_DESC[pos_axis]);
            gizmoAxis = pos_axis; // 0=X, 1=Y, 2=Z
        } else if (active_mode == MODE_ROT) {
            attach_text_description.setText("Rotation mode — " + ROT_DESC[rot_axis]);
            gizmoAxis = rot_axis;
        } else if (active_mode == MODE_SCALE) {
            attach_text_description.setText("Scale mode — Atur ukuran object");
            gizmoAxis = -1;
        }
        try { SetGizmoAxis(gizmoAxis); } catch (UnsatisfiedLinkError e) { /* native not loaded */ }
    }

    public void show()
    {
        isShow = true;
        // Default to position mode like PC edit object
        active_mode = MODE_POS;
        pos_axis = 0;
        rot_axis = 0;
        updateModeUI();
        Util.ShowLayout(attach_main_layot, true);
    }

    public void hide()
    {
        isShow = false;
        active_mode = -1;
        Util.HideLayout(attach_main_layot, true);
    }

    public void showWithoutReset()
    {
        attach_main_layot.setVisibility(View.VISIBLE);
    }

    public void hideWithoutReset()
    {
        attach_main_layot.setVisibility(View.GONE);
    }

    int fix_move = 0;
    private View.OnTouchListener touchListener = new View.OnTouchListener() {
        @Override public boolean onTouch(View view, MotionEvent event) {
            int btnType = getActiveButtonType();
            if (btnType < 0) return true;

            switch (event.getAction() & MotionEvent.ACTION_MASK) {
                case MotionEvent.ACTION_DOWN: {
                    fix_move = 10;
                    break;
                }
                case MotionEvent.ACTION_UP: {
                    if (view == attach_left_butt) {
                        AttachClick(btnType, false);
                    } else {
                        AttachClick(btnType, true);
                    }
                    break;
                }
                case MotionEvent.ACTION_MOVE: {
                    if(fix_move > 0){
                        fix_move --;
                        if(fix_move > 0)return true;
                    }
                    if(view == attach_left_butt){
                        AttachClick(btnType, false);
                    }else{
                        AttachClick(btnType, true);
                    }
                    break;
                }
            }
            return true;
        }
    };
}
