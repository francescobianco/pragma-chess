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
import androidx.compose.material.icons.outlined.Grid4x4
import androidx.compose.material.icons.outlined.PhoneAndroid
import androidx.compose.material.icons.outlined.Storage
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.DrawerValue
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalDrawerSheet
import androidx.compose.material3.ModalNavigationDrawer
import androidx.compose.material3.NavigationDrawerItem
import androidx.compose.material3.NavigationDrawerItemDefaults
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
import org.pragmachess.mobile.data.DatabaseRef

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
                    Text(stringResource(R.string.app_name), style = MaterialTheme.typography.titleLarge,
                        modifier = Modifier.padding(horizontal = 16.dp, vertical = 20.dp))
                    NavigationDrawerItem(
                        label = { Text(stringResource(R.string.board)) },
                        icon = { Icon(Icons.Outlined.Grid4x4, null) },
                        selected = vm.screens.isEmpty(),
                        onClick = { go { while (vm.back()) Unit } },
                    )
                    HorizontalDivider(Modifier.padding(vertical = 8.dp))
                    SectionTitle(stringResource(R.string.on_this_phone), Icons.Outlined.PhoneAndroid)
                    for (entry in vm.localDatabases) {
                        DatabaseItem(vm, entry.ref, entry.games) { go { vm.open(Screen.Games(entry.ref)) } }
                    }
                    NavigationDrawerItem(
                        label = { Text(stringResource(R.string.new_database)) },
                        icon = { Icon(Icons.Filled.Add, null) },
                        selected = false,
                        onClick = { go { newDatabase = true } },
                    )
                    for (computer in vm.computers) {
                        HorizontalDivider(Modifier.padding(vertical = 8.dp))
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            SectionTitle(computer.name, Icons.Filled.Computer, Modifier.weight(1f))
                            if (vm.syncStates[computer.pubkey] is SyncState.Running) {
                                CircularProgressIndicator(Modifier.padding(12.dp).size(20.dp), strokeWidth = 2.dp)
                            } else {
                                IconButton(onClick = { vm.sync(computer) }) { Icon(Icons.Filled.Sync, stringResource(R.string.sync_now)) }
                            }
                        }
                        for (entry in vm.computerDatabases[computer.pubkey].orEmpty()) {
                            DatabaseItem(vm, entry.ref, entry.games) { go { vm.open(Screen.Games(entry.ref)) } }
                        }
                    }
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

@Composable
private fun SectionTitle(text: String, icon: androidx.compose.ui.graphics.vector.ImageVector, modifier: Modifier = Modifier) {
    Row(modifier.padding(horizontal = 16.dp, vertical = 8.dp), verticalAlignment = Alignment.CenterVertically) {
        Icon(icon, null, Modifier.size(18.dp), tint = MaterialTheme.colorScheme.onSurfaceVariant)
        Spacer(Modifier.size(8.dp))
        Text(text, style = MaterialTheme.typography.titleSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
}

@Composable
private fun DatabaseItem(vm: AppViewModel, ref: DatabaseRef, games: Int, onClick: () -> Unit) {
    NavigationDrawerItem(
        label = { Text(ref.title) },
        icon = { Icon(Icons.Outlined.Storage, null) },
        badge = { Text(games.toString()) },
        selected = (vm.screens.lastOrNull() as? Screen.Games)?.ref == ref,
        onClick = onClick,
        modifier = Modifier.padding(NavigationDrawerItemDefaults.ItemPadding),
    )
}
