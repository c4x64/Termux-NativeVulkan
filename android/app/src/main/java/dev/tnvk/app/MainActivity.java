package dev.tnvk.app;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

/**
 * TNVK host activity: status header on top, Vulkan GUI surface in the
 * middle, CLI terminal docked below. Composition comes from
 * {@code res/layout/activity_main.xml} in the Zero Two midnight theme.
 */
public class MainActivity extends Activity {

    private TnvkView guiView;
    private TermView termView;
    private View statusDot;
    private TextView statusPill;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        NativeBridge.load();
        setContentView(R.layout.activity_main);

        guiView = findViewById(R.id.gui_view);
        termView = findViewById(R.id.term_view);
        statusDot = findViewById(R.id.status_dot);
        statusPill = findViewById(R.id.status_pill);

        // Runtime GUI check surfaces in the header pill and the CLI pane,
        // like a distro MOTD.
        String probe = probeGuiReady();
        boolean ready = probe != null && probe.startsWith("ready");
        updateStatus(ready, probe);

        termView.print("tnvk app \u2014 " + probe + "\n");
        termView.print("CLI inside GUI. Try: txnb install <wm>  |  txnb run-wm ziro-wm\n");

        termView.openShell(probeDefaultShell());

        if (ready) {
            guiView.start();
        }
    }

    private String probeGuiReady() {
        try {
            String probe = NativeBridge.guiReady();
            return probe != null ? probe : "missing: empty probe";
        } catch (UnsatisfiedLinkError e) {
            return "missing: " + e.getMessage();
        }
    }

    private String probeDefaultShell() {
        try {
            String shell = NativeBridge.defaultShell();
            return (shell == null || shell.isEmpty()) ? "/system/bin/sh" : shell;
        } catch (UnsatisfiedLinkError e) {
            return "/system/bin/sh";
        }
    }

    private void updateStatus(boolean ready, String probe) {
        if (statusDot != null) {
            statusDot.setBackgroundResource(ready ? R.drawable.dot_ready : R.drawable.dot_idle);
        }
        if (statusPill != null) {
            if (ready) {
                String shortName = probe.length() > 22 ? probe.substring(0, 22) + "\u2026" : probe;
                statusPill.setText(shortName.toUpperCase());
            } else {
                statusPill.setText("STANDBY");
            }
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (guiView != null) {
            guiView.onResume();
        }
    }

    @Override
    protected void onPause() {
        if (guiView != null) {
            guiView.onPause();
        }
        super.onPause();
    }
}
