package org.pragmachess.mobile.ui

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.ContentPaste
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.QrCodeScanner
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Sync
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.ListItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.delay
import org.pragmachess.mobile.BuildConfig
import org.pragmachess.mobile.R
import org.pragmachess.mobile.data.Computer
import org.pragmachess.mobile.data.DatabaseLocation
import org.pragmachess.mobile.data.DatabaseRef
import org.pragmachess.mobile.data.GameSummary
import org.pragmachess.mobile.link.SyncProgress
import java.text.DateFormat
import java.util.Date

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun ScreenScaffold(
    title: String,
    snackbar: SnackbarHostState,
    onBack: () -> Unit,
    actions: @Composable () -> Unit = {},
    fab: @Composable () -> Unit = {},
    content: @Composable (Modifier) -> Unit,
) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(title, maxLines = 1, overflow = TextOverflow.Ellipsis) },
                navigationIcon = {
                    IconButton(onClick = onBack) { Icon(Icons.AutoMirrored.Filled.ArrowBack, stringResource(R.string.back)) }
                },
                actions = { actions() },
            )
        },
        floatingActionButton = fab,
        snackbarHost = { SnackbarHost(snackbar) },
    ) { padding -> content(Modifier.padding(padding)) }
}

/** The games of one database, with a search on players and event. */
@Composable
fun GamesScreen(vm: AppViewModel, ref: DatabaseRef, snackbar: SnackbarHostState) {
    var filter by remember { mutableStateOf("") }
    var games by remember { mutableStateOf<List<GameSummary>?>(null) }
    var menu by remember { mutableStateOf(false) }
    var confirmDelete by remember { mutableStateOf(false) }
    LaunchedEffect(ref, filter, vm.computerDatabases, vm.localDatabases) {
        delay(150)
        games = vm.games(ref, filter)
    }
    ScreenScaffold(
        title = vm.databaseLabel(ref),
        snackbar = snackbar,
        onBack = { vm.back() },
        actions = {
            if (ref.location == DatabaseLocation.Local) {
                Box {
                    IconButton(onClick = { menu = true }) { Icon(Icons.Filled.MoreVert, stringResource(R.string.more)) }
                    DropdownMenu(expanded = menu, onDismissRequest = { menu = false }) {
                        DropdownMenuItem(
                            text = { Text(stringResource(R.string.delete_database)) },
                            leadingIcon = { Icon(Icons.Filled.Delete, null) },
                            onClick = {
                                menu = false
                                confirmDelete = true
                            },
                        )
                    }
                }
            }
        },
        fab = {
            ExtendedFloatingActionButton(
                text = { Text(stringResource(R.string.new_game)) },
                icon = { Icon(Icons.Filled.Add, null) },
                onClick = { vm.newGame(ref) },
            )
        },
    ) { modifier ->
        Column(modifier.fillMaxSize()) {
            OutlinedTextField(
                value = filter,
                onValueChange = { filter = it },
                placeholder = { Text(stringResource(R.string.search_games)) },
                leadingIcon = { Icon(Icons.Filled.Search, null) },
                singleLine = true,
                modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 8.dp),
            )
            val list = games
            when {
                list == null -> Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) { CircularProgressIndicator() }
                list.isEmpty() -> Box(Modifier.fillMaxSize().padding(32.dp), contentAlignment = Alignment.Center) {
                    Text(stringResource(if (filter.isBlank()) R.string.no_games else R.string.no_games_found),
                        style = MaterialTheme.typography.bodyLarge, color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
                else -> LazyColumn(Modifier.fillMaxSize()) {
                    items(list, key = { it.id }) { game ->
                        ListItem(
                            headlineContent = {
                                Text("${game.white.ifBlank { "?" }} – ${game.black.ifBlank { "?" }}", maxLines = 1, overflow = TextOverflow.Ellipsis)
                            },
                            supportingContent = {
                                Text(listOf(game.event, game.date).filter { it.isNotBlank() }.joinToString(" · "),
                                    maxLines = 1, overflow = TextOverflow.Ellipsis)
                            },
                            trailingContent = { Text(game.result.ifBlank { "*" }, style = MaterialTheme.typography.labelLarge) },
                            modifier = Modifier.clickable { vm.openGame(ref, game.id) },
                        )
                        HorizontalDivider()
                    }
                    item { Spacer(Modifier.height(88.dp)) }
                }
            }
        }
    }
    if (confirmDelete) {
        ConfirmDialog(
            title = stringResource(R.string.delete_database),
            text = stringResource(R.string.delete_database_text, ref.title),
            confirm = stringResource(R.string.delete),
            onConfirm = { vm.deleteDatabase(ref) },
            onDismiss = { confirmDelete = false },
        )
    }
}

/** The paired computers, how to pair one, and the state of their syncs. */
@Composable
fun ComputersScreen(vm: AppViewModel, snackbar: SnackbarHostState, onScan: () -> Unit, onPaste: () -> Unit) {
    var forgetting by remember { mutableStateOf<Computer?>(null) }
    ScreenScaffold(stringResource(R.string.computers), snackbar, onBack = { vm.back() }) { modifier ->
        LazyColumn(modifier.fillMaxSize(), contentPadding = androidx.compose.foundation.layout.PaddingValues(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)) {
            item {
                Card(Modifier.fillMaxWidth()) {
                    Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                        Text(stringResource(R.string.connect_computer), style = MaterialTheme.typography.titleMedium)
                        Text(stringResource(R.string.pair_instructions), style = MaterialTheme.typography.bodyMedium)
                        androidx.compose.foundation.layout.FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            Button(onClick = onScan) {
                                Icon(Icons.Filled.QrCodeScanner, null)
                                Spacer(Modifier.size(8.dp))
                                Text(stringResource(R.string.scan_code))
                            }
                            OutlinedButton(onClick = onPaste) {
                                Icon(Icons.Filled.ContentPaste, null)
                                Spacer(Modifier.size(8.dp))
                                Text(stringResource(R.string.paste_link))
                            }
                        }
                    }
                }
            }
            items(vm.computers, key = { it.pubkey }) { computer ->
                ComputerCard(vm, computer, onForget = { forgetting = computer }, onScan = onScan)
            }
        }
    }
    forgetting?.let { computer ->
        ConfirmDialog(
            title = stringResource(R.string.forget_computer),
            text = stringResource(R.string.forget_computer_text, computer.name),
            confirm = stringResource(R.string.forget),
            onConfirm = { vm.forget(computer) },
            onDismiss = { forgetting = null },
        )
    }
}

@Composable
private fun ComputerCard(vm: AppViewModel, computer: Computer, onForget: () -> Unit, onScan: () -> Unit) {
    val state = vm.syncStates[computer.pubkey] ?: SyncState.Idle
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    Text(computer.name, style = MaterialTheme.typography.titleMedium)
                    val databases = vm.computerDatabases[computer.pubkey].orEmpty().size
                    val last = if (computer.lastSync > 0)
                        stringResource(R.string.last_sync, DateFormat.getDateTimeInstance(DateFormat.SHORT, DateFormat.SHORT).format(Date(computer.lastSync)))
                    else stringResource(R.string.never_synced)
                    Text("$last · ${pluralDatabases(databases)}", style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant)
                    val waiting = vm.outboxCount(computer)
                    if (waiting > 0) {
                        Text(stringResource(R.string.games_waiting, waiting), style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.primary)
                    }
                }
                IconButton(onClick = { vm.sync(computer) }, enabled = state !is SyncState.Running) {
                    Icon(Icons.Filled.Sync, stringResource(R.string.sync_now))
                }
                IconButton(onClick = onForget) { Icon(Icons.Filled.Delete, stringResource(R.string.forget)) }
            }
            when (state) {
                is SyncState.Running -> {
                    Text(progressText(state.progress), style = MaterialTheme.typography.bodySmall)
                    val fraction = state.progress.fraction
                    if (fraction != null) LinearProgressIndicator(progress = { fraction }, modifier = Modifier.fillMaxWidth())
                    else LinearProgressIndicator(Modifier.fillMaxWidth())
                }
                is SyncState.Failed -> {
                    Text(vm.failureText(state.failure, computer.name), style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.error)
                    if (state.failure == org.pragmachess.mobile.link.SyncFailure.Refused) {
                        TextButton(onClick = onScan) { Text(stringResource(R.string.scan_again)) }
                    }
                }
                is SyncState.Done -> {
                    val r = state.result
                    Text(stringResource(R.string.sync_done, r.pushed, r.received.size), style = MaterialTheme.typography.bodySmall)
                }
                SyncState.Idle -> Unit
            }
        }
    }
}

@Composable
private fun pluralDatabases(count: Int): String =
    androidx.compose.ui.res.pluralStringResource(R.plurals.databases, count, count)

@Composable
fun progressText(progress: SyncProgress): String = when (progress.step) {
    SyncProgress.Step.Relays -> stringResource(R.string.progress_relays)
    SyncProgress.Step.Offer -> stringResource(R.string.progress_offer)
    SyncProgress.Step.Connecting -> stringResource(R.string.progress_connecting)
    SyncProgress.Step.Sending -> stringResource(R.string.progress_sending, progress.file.orEmpty())
    SyncProgress.Step.Listing -> stringResource(R.string.progress_listing)
    SyncProgress.Step.Receiving -> stringResource(R.string.progress_receiving, progress.file.orEmpty().removeSuffix(".pdb"))
}

/** A field to paste the pairing link into, for when the camera is not an option. */
@Composable
fun PasteLinkDialog(onPair: (String) -> Unit, onDismiss: () -> Unit) {
    var text by remember { mutableStateOf("") }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(R.string.paste_link)) },
        text = {
            OutlinedTextField(text, { text = it }, label = { Text("pragma-chess://pair?…") }, minLines = 3)
        },
        confirmButton = {
            TextButton(enabled = text.isNotBlank(), onClick = {
                onPair(text)
                onDismiss()
            }) { Text(stringResource(R.string.connect)) }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text(stringResource(android.R.string.cancel)) } },
    )
}

@Composable
fun SettingsScreen(vm: AppViewModel, snackbar: SnackbarHostState) {
    var name by remember { mutableStateOf(vm.phoneName) }
    ScreenScaffold(stringResource(R.string.settings), snackbar, onBack = {
        vm.setPhoneName(name)
        vm.back()
    }) { modifier ->
        Column(modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)) {
            OutlinedTextField(
                value = name,
                onValueChange = { name = it },
                label = { Text(stringResource(R.string.phone_name)) },
                supportingText = { Text(stringResource(R.string.phone_name_help)) },
                singleLine = true,
                modifier = Modifier.fillMaxWidth(),
            )
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Text(stringResource(R.string.engine), style = MaterialTheme.typography.titleSmall)
                Text(stringResource(if (vm.engineAvailable) R.string.engine_available else R.string.engine_unavailable),
                    style = MaterialTheme.typography.bodyMedium)
            }
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Text(stringResource(R.string.phone_key), style = MaterialTheme.typography.titleSmall)
                Text(vm.phoneKey, style = MaterialTheme.typography.bodySmall, fontFamily = FontFamily.Monospace)
            }
            HorizontalDivider()
            ListItem(
                headlineContent = { Text(stringResource(R.string.about_licenses)) },
                supportingContent = { Text(stringResource(R.string.about_title, BuildConfig.VERSION_NAME)) },
                modifier = Modifier.clickable {
                    vm.setPhoneName(name)
                    vm.open(Screen.Licenses)
                },
            )
        }
    }
}

/** The version and the licenses of what the app bundles; Stockfish's GPL text in full. */
@Composable
fun LicensesScreen(vm: AppViewModel, snackbar: SnackbarHostState) {
    val context = androidx.compose.ui.platform.LocalContext.current
    val stockfishLicense = remember {
        runCatching { context.assets.open("licenses/stockfish-COPYING.txt").bufferedReader().use { it.readText() } }.getOrNull()
    }
    var showGpl by remember { mutableStateOf(false) }
    val uri = androidx.compose.ui.platform.LocalUriHandler.current
    ScreenScaffold(stringResource(R.string.about_licenses), snackbar, onBack = { vm.back() }) { modifier ->
        Column(modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)) {
            Text(stringResource(R.string.about_title, BuildConfig.VERSION_NAME), style = MaterialTheme.typography.titleMedium)
            Text(stringResource(R.string.about_app), style = MaterialTheme.typography.bodyMedium)
            HorizontalDivider()
            LicenseEntry(
                title = stringResource(R.string.license_stockfish_title, BuildConfig.STOCKFISH_RELEASE.removePrefix("sf_")),
                text = stringResource(R.string.license_stockfish_text),
                link = "https://github.com/official-stockfish/Stockfish/tree/${BuildConfig.STOCKFISH_RELEASE}",
                onLink = uri::openUri,
            )
            if (stockfishLicense != null) {
                TextButton(onClick = { showGpl = !showGpl }) {
                    Text(stringResource(if (showGpl) R.string.license_hide else R.string.license_show_gpl))
                }
                if (showGpl) {
                    Text(stockfishLicense, style = MaterialTheme.typography.bodySmall, fontFamily = FontFamily.Monospace)
                }
            }
            LicenseEntry(stringResource(R.string.license_pieces_title), stringResource(R.string.license_pieces_text),
                "http://www.enpassant.dk/chess/fonteng.htm#GC", uri::openUri)
            LicenseEntry(stringResource(R.string.license_figurines_title), stringResource(R.string.license_figurines_text),
                "https://ctan.org/pkg/skaknew", uri::openUri)
            LicenseEntry(stringResource(R.string.license_libraries_title), stringResource(R.string.license_libraries_text), null, uri::openUri)
        }
    }
}

@Composable
private fun LicenseEntry(title: String, text: String, link: String?, onLink: (String) -> Unit) {
    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Text(title, style = MaterialTheme.typography.titleSmall)
        Text(text, style = MaterialTheme.typography.bodySmall)
        if (link != null) {
            Text(link, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.primary,
                modifier = Modifier.clickable { onLink(link) })
        }
    }
}
