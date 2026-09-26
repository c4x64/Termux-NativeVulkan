package dev.tnvk.app;

import android.content.Context;
import android.graphics.Typeface;
import android.text.method.ScrollingMovementMethod;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

/**
 * CLI UI inside the GUI: output pane + input line wired to a pty whose
 * slave runs `txnb term`. Distro-style installs go through
 * `txnb install &lt;wm&gt;` so WMs/tools land like on normal Linux.
 *
 * <p>Visuals come from {@code res/layout/view_term.xml} plus the Zero Two
 * midnight/sakura theme: padded monospace output pane with a pink
 * scrollbar, and a styled input bar whose SEND button shares the exact
 * Enter-key submit path.
 */
public final class TermView extends LinearLayout {

    private TextView output;
    private EditText input;
    private ScrollView scroll;
    private Button send;
    private TermSession session;

    public TermView(Context ctx) {
        super(ctx);
        init(ctx);
    }

    public TermView(Context ctx, AttributeSet attrs) {
        super(ctx, attrs);
        init(ctx);
    }

    public TermView(Context ctx, AttributeSet attrs, int defStyleAttr) {
        super(ctx, attrs, defStyleAttr);
        init(ctx);
    }

    private void init(Context ctx) {
        setOrientation(VERTICAL);
        LayoutInflater.from(ctx).inflate(R.layout.view_term, this, true);

        scroll = findViewById(R.id.term_scroll);
        output = findViewById(R.id.term_output);
        input = findViewById(R.id.term_input);
        send = findViewById(R.id.term_send);

        output.setTypeface(Typeface.MONOSPACE);
        output.setMovementMethod(new ScrollingMovementMethod());

        input.setTypeface(Typeface.MONOSPACE);
        input.setSingleLine(true);
        input.setImeOptions(EditorInfo.IME_ACTION_DONE);
        input.setOnEditorActionListener((v, actionId, event) -> {
            boolean enterKey = event != null
                    && event.getKeyCode() == KeyEvent.KEYCODE_ENTER
                    && event.getAction() == KeyEvent.ACTION_DOWN;
            if (actionId == EditorInfo.IME_ACTION_DONE
                    || actionId == EditorInfo.IME_ACTION_SEND
                    || enterKey) {
                submit(input.getText().toString());
                return true;
            }
            return false;
        });

        send.setOnClickListener(v -> submit(input.getText().toString()));

        setFocusableInTouchMode(true);
    }

    /** Spawn the shell; show the txnb hint on first launch. */
    public void openShell(String shell) {
        String target = (shell == null || shell.isEmpty()) ? "/system/bin/sh" : shell;
        session = new TermSession(target, this::append);
        session.start();
    }

    /** Distro-style entry: `txnb install &lt;wm&gt;` handled natively. */
    private void submit(String line) {
        if (line == null) {
            line = "";
        }
        append("$ " + line + "\n");
        input.setText("");
        String t = line.trim();
        if (t.startsWith("txnb install ")) {
            String rest = t.substring("txnb install ".length()).trim();
            if (!rest.isEmpty()) {
                String[] pkgs = rest.split("\\s+");
                int rc = NativeBridge.installPackages(pkgs);
                append("[txnb install] exit=" + rc + "\n");
            } else {
                append("usage: txnb install <wm> [tools...]\n");
            }
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
        if (s == null || s.isEmpty()) {
            return;
        }
        post(() -> {
            output.append(s);
            if (scroll != null) {
                scroll.post(() -> scroll.fullScroll(FOCUS_DOWN));
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
        private volatile int writer = -1;

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
            writer = fds[1];
            int reader = fds[0];
            byte[] buf = new byte[4096];
            for (;;) {
                int n = NativePty.read(reader, buf);
                if (n <= 0) {
                    sink.onOutput("\n[term] shell exited\n");
                    return;
                }
                sink.onOutput(new String(buf, 0, n));
            }
        }

        void write(String s) {
            int fd = writer;
            if (fd >= 0 && s != null) {
                NativePty.write(fd, s.getBytes());
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
