package dev.tnvk.app;

import android.app.Activity;
import android.os.Bundle;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

/** TNVK host activity: GUI surface on top, CLI terminal docked below. */
public class MainActivity extends Activity {

    private TnvkView guiView;
    private TermView termView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        NativeBridge.load();

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);

        guiView = new TnvkView(this);
        termView = new TermView(this);

        LinearLayout.LayoutParams guiParams =
                new LinearLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT, 0, 3f);
        LinearLayout.LayoutParams termParams =
                new LinearLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT, 0, 2f);

        root.addView(guiView, guiParams);
        root.addView(termView, termParams);
        setContentView(root);

        // Runtime GUI check surfaces in the CLI pane, like a distro MOTD.
        String probe = NativeBridge.guiReady();
        termView.print("tnvk app — " + probe + "\n");
        termView.print("CLI inside GUI. Try: txnb install <wm>  |  txnb run-wm ziro-wm\n");
    }

    @Override
    protected void onResume() {
        super.onResume();
        guiView.onResume();
    }

    @Override
    protected void onPause() {
        guiView.onPause();
        super.onPause();
    }
}
