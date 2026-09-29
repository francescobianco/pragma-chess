package org.pragmachess.mobile

import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import org.pragmachess.mobile.ui.AppViewModel
import org.pragmachess.mobile.ui.PragmaApp
import org.pragmachess.mobile.ui.PragmaTheme

class MainActivity : ComponentActivity() {
    private val viewModel: AppViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        setContent { PragmaTheme { PragmaApp(viewModel) } }
        if (savedInstanceState == null) handle(intent)
    }

    override fun onStart() {
        super.onStart()
        viewModel.setForeground(true)
    }

    override fun onStop() {
        viewModel.setForeground(false)
        super.onStop()
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        handle(intent)
    }

    /** A pairing link opened from elsewhere (a scanned QR code in the camera app, a message). */
    private fun handle(intent: Intent?) {
        val data = intent?.data ?: return
        if (intent.action == Intent.ACTION_VIEW && data.scheme == "pragma-chess") viewModel.pair(data.toString())
    }
}
