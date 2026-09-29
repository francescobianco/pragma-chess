package org.pragmachess.mobile.ui

import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.layout.Column
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Computer
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Sync
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.DrawerValue
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalDrawerSheet
import androidx.compose.material3.ModalNavigationDrawer
import androidx.compose.material3.NavigationDrawerItem
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.material3.rememberDrawerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.journeyapps.barcodescanner.ScanContract
import com.journeyapps.barcodescanner.ScanOptions
import kotlinx.coroutines.launch
import org.pragmachess.mobile.R
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.sp
import org.pragmachess.mobile.data.CorpusEntry

@Composable
fun PragmaApp(vm: AppViewModel) {
    val drawer = rememberDrawerState(DrawerValue.Closed)
    val scope = rememberCoroutineScope()
    val snackbar = remember { SnackbarHostState() }
    var newDatabase by remember { mutableStateOf(false) }
    var pasteLink by remember { mutableStateOf(false) }
    val scanPrompt = stringResource(R.string.scan_prompt)
    val scanner = rememberLauncherForActivityResult(ScanContract()) { result ->
        result.contents?.let(vm::pair)
    }
    val scan = {
        scanner.launch(ScanOptions().setDesiredBarcodeFormats(ScanOptions.QR_CODE).setPrompt(scanPrompt)
            .setBeepEnabled(false).setOrientationLocked(false))
    }

    LaunchedEffect(vm.message) {
        vm.message?.let {
            snackbar.showSnackbar(it)
            vm.message = null
        }
    }

    BackHandler(enabled = drawer.isOpen) { scope.launch { drawer.close() } }
    BackHandler(enabled = !drawer.isOpen && vm.screens.isNotEmpty()) { vm.back() }

    fun go(action: () -> Unit) {
        action()
        scope.launch { drawer.close() }
    }

    ModalNavigationDrawer(
        drawerState = drawer,
        drawerContent = {
            ModalDrawerSheet {
                Column(Modifier.verticalScroll(rememberScrollState()).padding(horizontal = 12.dp)) {
                    DrawerHeader(vm)
                    NavigationDrawerItem(
                        label = { Text(stringResource(R.string.board)) },
                        icon = { Icon(PragmaIcons.Board, null) },
                        selected = vm.screens.isEmpty(),
                        onClick = { go { while (vm.back()) Unit } },
                    )
                    HorizontalDivider(Modifier.padding(vertical = 8.dp))
                    // One corpus: every database in one list, whoever made it.
                    for (entry in vm.databases) {
                        DatabaseItem(vm, entry) { go { vm.open(Screen.Games(entry.ref)) } }
                    }
                    NavigationDrawerItem(
                        label = { Text(stringResource(R.string.new_database)) },
                        icon = { Icon(Icons.Filled.Add, null) },
                        selected = false,
                        onClick = { go { newDatabase = true } },
                    )
                    HorizontalDivider(Modifier.padding(vertical = 8.dp))
                    NavigationDrawerItem(
                        label = { Text(stringResource(R.string.computers)) },
                        icon = { Icon(Icons.Filled.Computer, null) },
                        selected = vm.screens.lastOrNull() == Screen.Computers,
                        onClick = { go { vm.open(Screen.Computers) } },
                    )
                    NavigationDrawerItem(
                        label = { Text(stringResource(R.string.settings)) },
                        icon = { Icon(Icons.Filled.Settings, null) },
                        selected = vm.screens.lastOrNull() == Screen.Settings,
                        onClick = { go { vm.open(Screen.Settings) } },
                    )
                    Spacer(Modifier.height(16.dp))
                }
            }
        },
    ) {
        when (val screen = vm.screens.lastOrNull()) {
            null -> BoardScreen(vm, snackbar, onMenu = { scope.launch { drawer.open() } })
            is Screen.Games -> GamesScreen(vm, screen.ref, snackbar)
            Screen.Computers -> ComputersScreen(vm, snackbar, onScan = scan, onPaste = { pasteLink = true })
            Screen.Settings -> SettingsScreen(vm, snackbar)
            Screen.Licenses -> LicensesScreen(vm, snackbar)
        }
    }

    if (newDatabase) {
        NewDatabaseDialog(onCreate = { name -> vm.createDatabase(name) { vm.open(Screen.Games(it)) } },
            onDismiss = { newDatabase = false })
    }
    if (pasteLink) PasteLinkDialog(onPair = vm::pair, onDismiss = { pasteLink = false })
    vm.pendingDiscard?.let { action ->
        ConfirmDialog(
            title = stringResource(R.string.discard_game),
            text = stringResource(R.string.discard_game_text),
            confirm = stringResource(R.string.discard),
            onConfirm = action,
            onDismiss = { vm.pendingDiscard = null },
        )
    }
}

/** The logo, the name with a small green BETA, and the sync of every computer. */
@Composable
private fun DrawerHeader(vm: AppViewModel) {
    Row(Modifier.padding(start = 16.dp, end = 4.dp, top = 16.dp, bottom = 12.dp), verticalAlignment = Alignment.CenterVertically) {
        Image(painterResource(R.mipmap.ic_launcher), null, Modifier.size(36.dp))
        Spacer(Modifier.width(12.dp))
        Row(Modifier.weight(1f), verticalAlignment = Alignment.Top) {
            Text(stringResource(R.string.app_name), style = MaterialTheme.typography.titleLarge)
            // A superscript: small, raised to the top of the title.
            Text(
                stringResource(R.string.beta),
                color = Color.White,
                fontSize = 7.sp,
                lineHeight = 8.sp,
                fontWeight = FontWeight.Bold,
                letterSpacing = 0.4.sp,
                modifier = Modifier
                    .padding(start = 3.dp, top = 1.dp)
                    .background(BetaGreen, RoundedCornerShape(3.dp))
                    .padding(horizontal = 3.dp, vertical = 1.dp),
            )
        }
        if (vm.computers.isNotEmpty()) {
            if (vm.computers.any { vm.syncStates[it.pubkey] is SyncState.Running }) {
                CircularProgressIndicator(Modifier.padding(12.dp).size(20.dp), strokeWidth = 2.dp)
            } else {
                IconButton(onClick = { vm.syncAll() }) { Icon(Icons.Filled.Sync, stringResource(R.string.sync_now)) }
            }
        }
    }
}

private val BetaGreen = Color(0xFF2E7D32)

/** A database, aligned with the other items; the device it came from when another one has the same name. */
@Composable
private fun DatabaseItem(vm: AppViewModel, entry: CorpusEntry, onClick: () -> Unit) {
    NavigationDrawerItem(
        label = {
            Column {
                Text(vm.displayTitle(entry), maxLines = 1, overflow = TextOverflow.Ellipsis)
                if (vm.homonyms(entry) && entry.origin.isNotBlank()) {
                    Text(entry.origin, style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 1)
                }
            }
        },
        icon = { Icon(PragmaIcons.Database, null) },
        badge = { Text(entry.games.toString()) },
        selected = (vm.screens.lastOrNull() as? Screen.Games)?.ref == entry.ref,
        onClick = onClick,
    )
}
