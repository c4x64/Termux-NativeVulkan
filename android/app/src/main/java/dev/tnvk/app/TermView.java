package dev.tnvk.app;

import android.content.Context;
import android.graphics.Color;
import android.text.method.ScrollingMovementMethod;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.FileDescriptor;

/**
 * CLI UI inside the GUI: output pane + input line wired to a pty whose
 * slave runs `txnb term`. Distro-style installs go through
 * `txnb install <wm>` so WMs/tools land like on normal Linux.
 */
public final class TermView extends LinearLayout {

    private final TextView output;
    private final EditText input;
    private TermSession session;

    public TermView(Context ctx) {
        super(ctx);
        setOrientation(VERTICAL);
        setBackgroundColor(Color.parseColor("#0d0716"));

        output = new TextView(ctx);
        output.setTextColor(Color.parseColor("#ffd7e6"));
        output.setTextSize(12f);
        output.setMovementMethod(new ScrollingMovementMethod());
        ScrollView scroll = new ScrollView(ctx);
        scroll.addView(output);
        LinearLayout.LayoutParams scrollParams =
                new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, 0, 1f);
        addView(scroll, scrollParams);

        input = new EditText(ctx);
        input.setHint("txnb install <wm>  |  txnb run-wm ziro-wm");
        input.setTextColor(Color.parseColor("#ffd7e6"));
        input.setHintTextColor(Color.parseColor("#3a1d2e"));
        input.setBackgroundColor(Color.parseColor("#1a0f24"));
        input.setSingleLine(true);
        input.setOnEditorActionListener((v, actionId, event) -> {
            if (event != null && event.getAction() == KeyEvent.ACTION_DOWN) {
                submit(input.getText().toString());
                return true;
            }
            return false;
        });
        addView(input, new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        setFocusableInTouchMode(true);
    }

    /** Spawn the shell; show the txnb hint on first launch. */
    public void openShell(String shell) {
        session = new TermSession(shell, this::append);
        session.start();
    }

    /** Distro-style entry: `txnb install <wm>` handled natively. */
    private void submit(String line) {
        append("$ " + line + "\n");
        input.setText("");
        String t = line.trim();
        if (t.startsWith("txnb install ")) {
            String[] pkgs = t.substring("txnb install ".length()).trim().split("\\s+");
            int rc = NativeBridge.installPackages(pkgs);
            append("[txnb install] exit=" + rc + "\n");
            return;
        }
        if (session != null) {
            session.write(line + "\n");
        }
    }

    public void print(String s) {
        append(s);
    }

    private void append(final String s) {
        post(() -> {
            output.append(s);
            View parent = (View) output.getParent();
            if (parent instanceof ScrollView) {
                ((ScrollView) parent).fullScroll(FOCUS_DOWN);
            }
        });
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        outAttrs.actionLabel = null;
        outAttrs.inputType = EditorInfo.TYPE_CLASS_TEXT;
        outAttrs.imeOptions = EditorInfo.IME_ACTION_DONE;
        return new BaseInputConnection(this, true);
    }

    /** Minimal pty session: master FD pumped to the output pane. */
    private static final class TermSession extends Thread {
        private final String shell;
        private final Sink sink;
        private FileDescriptor master;
        private int writer = -1;

        interface Sink {
            void onOutput(String s);
        }

        TermSession(String shell, Sink sink) {
            this.shell = shell;
            this.sink = sink;
            setDaemon(true);
        }

        @Override
        public void run() {
            int[] fds = NativePty.open(shell);
            if (fds == null || fds.length < 2) {
                sink.onOutput("[term] pty failed\n");
                return;
            }
            byte[] buf = new byte[4096];
            for (;;) {
                int n = NativePty.read(fds[0], buf);
                if (n <= 0) {
                    sink.onOutput("\n[term] shell exited\n");
                    return;
                }
                sink.onOutput(new String(buf, 0, n));
            }
        }

        void write(String s) {
            if (writer >= 0) {
                NativePty.write(writer, s.getBytes());
            }
        }
    }

    /** JNI pty helpers implemented in cpp/native-lib.cpp. */
    private static final class NativePty {
        static int[] open(String shell) {
            return nativeOpen(shell);
        }

        static int read(int fd, byte[] buf) {
            return nativeRead(fd, buf);
        }

        static int write(int fd, byte[] data) {
            return nativeWrite(fd, data);
        }

        private static native int[] nativeOpen(String shell);

        private static native int nativeRead(int fd, byte[] buf);

        private static native int nativeWrite(int fd, byte[] data);
    }
}
