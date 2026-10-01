package com.sampmobile.xyvern.game.ui;

import android.app.Activity;
import android.view.View;
import android.widget.ProgressBar;
import androidx.constraintlayout.widget.ConstraintLayout;
import com.sampmobile.xyvern.R;

public class LoadingScreen {

    private Activity activity;
    private ConstraintLayout mainLayout;
    private ProgressBar progressBar;

    public LoadingScreen(Activity activity) {
        this.activity = activity;

        mainLayout = (ConstraintLayout) activity.getLayoutInflater()
                .inflate(R.layout.loadingscreen, null);

        progressBar = mainLayout.findViewById(R.id.progressBar2);

        activity.addContentView(mainLayout,
                new ConstraintLayout.LayoutParams(-1, -1));
    }

    public void setProgress(int value) {
        activity.runOnUiThread(() -> {
            if (progressBar != null) {
                progressBar.setProgress(value);
            }
        });
    }

    public void show() {
        activity.runOnUiThread(() -> {
            if (mainLayout != null) {
                mainLayout.setVisibility(View.VISIBLE);
            }
        });
    }

    public void hide() {
        activity.runOnUiThread(() -> {
            if (mainLayout != null) {
                mainLayout.setVisibility(View.INVISIBLE);
            }
        });
    }
}
