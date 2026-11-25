package openMapleClient

import android.app.NativeActivity
import android.content.Context
import android.os.Bundle
import android.view.WindowInsets
import android.view.WindowInsetsAnimation
import android.view.WindowManager
import android.view.inputmethod.InputMethodManager

class MapleActivity : NativeActivity() {
    private var lastImeVisible = false

    private external fun nativeRegisterActivity()
    private external fun nativeUnregisterActivity()
    private external fun nativeOnImeVisibilityChanged(visible: Boolean)

    @Suppress("unused")
    fun setImeVisibleFromNative(visible: Boolean) {
        runOnUiThread {
            val decorView = window.decorView
            val imm = getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
            if (visible) {
                decorView.windowInsetsController?.show(WindowInsets.Type.ime())
                imm.showSoftInput(decorView, 0)
            } else {
                decorView.windowInsetsController?.hide(WindowInsets.Type.ime())
                imm.hideSoftInputFromWindow(decorView.windowToken, 0)
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        nativeRegisterActivity()
        window.setSoftInputMode(
            WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING
                    or WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN
        )
        val decorView = window.decorView
        val imm = getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
        imm.hideSoftInputFromWindow(decorView.windowToken, 0)

        decorView.setWindowInsetsAnimationCallback(
            object : WindowInsetsAnimation.Callback(DISPATCH_MODE_CONTINUE_ON_SUBTREE) {
                private var sawProgress = false
                private var lastImeBottom = 0

                override fun onPrepare(animation: WindowInsetsAnimation) {
                    if ((animation.typeMask and WindowInsets.Type.ime()) != 0) {
                        sawProgress = false
                    }
                }

                override fun onProgress(
                    insets: WindowInsets,
                    runningAnimations: MutableList<WindowInsetsAnimation>
                ): WindowInsets {
                    if (runningAnimations.any { (it.typeMask and WindowInsets.Type.ime()) != 0 }) {
                        sawProgress = true
                        lastImeBottom = insets.getInsets(WindowInsets.Type.ime()).bottom
                    }
                    return insets
                }

                override fun onEnd(animation: WindowInsetsAnimation) {
                    if ((animation.typeMask and WindowInsets.Type.ime()) != 0) {
                        val visible = if (sawProgress) {
                            lastImeBottom > 0
                        } else {
                            decorView.rootWindowInsets?.isVisible(WindowInsets.Type.ime()) == true
                        }
                        if (visible != lastImeVisible) {
                            lastImeVisible = visible
                            nativeOnImeVisibilityChanged(visible)
                        }
                    }
                }
            }
        )
    }

    override fun onDestroy() {
        nativeUnregisterActivity()
        super.onDestroy()
    }

    companion object {
        init {
            System.loadLibrary("OpenMapleClient")
        }
    }
}
