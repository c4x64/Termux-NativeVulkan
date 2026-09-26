package dev.tnvk.app;

import android.content.Context;
import android.util.AttributeSet;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/**
 * Runtime GUI surface. The Vulkan swapchain is created natively once
 * {@link NativeBridge#isGuiReady()} is true; until then the view stays idle
 * so a missing driver can never black-screen the app.
 */
public final class TnvkView extends SurfaceView implements SurfaceHolder.Callback {

    private boolean started;

    public TnvkView(Context ctx) {
        super(ctx);
        init();
    }

    public TnvkView(Context ctx, AttributeSet attrs) {
        super(ctx, attrs);
        init();
    }

    public TnvkView(Context ctx, AttributeSet attrs, int defStyleAttr) {
        super(ctx, attrs, defStyleAttr);
        init();
    }

    private void init() {
        getHolder().addCallback(this);
        setFocusable(true);
        setFocusableInTouchMode(true);
    }

    /** Called by MainActivity after the HW check. */
    public void start() {
        started = true;
        if (getHolder().getSurface().isValid()) {
            nativeStart(getHolder().getSurface());
        }
    }

    public void onResume() {
        if (started && getHolder().getSurface().isValid()) {
            nativeStart(getHolder().getSurface());
        }
    }

    public void onPause() {
        nativeStop();
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        if (started) {
            nativeStart(holder.getSurface());
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {
        nativeResize(w, h);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        nativeStop();
    }

    private static native void nativeStart(Object surface);

    private static native void nativeResize(int w, int h);

    private static native void nativeStop();
}
