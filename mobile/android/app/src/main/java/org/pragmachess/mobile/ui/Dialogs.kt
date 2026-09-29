package org.pragmachess.mobile.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MenuAnchorType
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardCapitalization
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import org.pragmachess.mobile.R
import org.pragmachess.mobile.data.GameRecord

/** The headers of the game on the board and the database to save it to. */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun GameDetailsDialog(vm: AppViewModel, onDismiss: () -> Unit) {
    val h = vm.headers
    var white by remember { mutableStateOf(h.white) }
    var black by remember { mutableStateOf(h.black) }
    var event by remember { mutableStateOf(h.event) }
    var site by remember { mutableStateOf(h.site) }
    var date by remember { mutableStateOf(h.date) }
    var round by remember { mutableStateOf(h.round) }
    var result by remember { mutableStateOf(h.result) }
    val databases = vm.writableDatabases
    var database by remember { mutableStateOf(vm.gameDatabase?.takeIf { it in databases } ?: databases.firstOrNull()) }
    var picking by remember { mutableStateOf(false) }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(R.string.save_game)) },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                val words = KeyboardOptions(capitalization = KeyboardCapitalization.Words)
                OutlinedTextField(white, { white = it }, label = { Text(stringResource(R.string.white)) }, singleLine = true, keyboardOptions = words)
                OutlinedTextField(black, { black = it }, label = { Text(stringResource(R.string.black)) }, singleLine = true, keyboardOptions = words)
                FlowRow(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    for (value in GameRecord.RESULTS) {
                        FilterChip(selected = result == value, onClick = { result = value }, label = { Text(value) })
                    }
                }
                OutlinedTextField(event, { event = it }, label = { Text(stringResource(R.string.event)) }, singleLine = true, keyboardOptions = words)
                OutlinedTextField(site, { site = it }, label = { Text(stringResource(R.string.site)) }, singleLine = true, keyboardOptions = words)
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedTextField(date, { date = it }, label = { Text(stringResource(R.string.date)) }, singleLine = true,
                        modifier = Modifier.weight(2f))
                    OutlinedTextField(round, { round = it }, label = { Text(stringResource(R.string.round)) }, singleLine = true,
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number), modifier = Modifier.weight(1f))
                }
                ExposedDropdownMenuBox(expanded = picking, onExpandedChange = { picking = it }) {
                    OutlinedTextField(
                        value = database?.let(vm::databaseLabel).orEmpty(),
                        onValueChange = {},
                        readOnly = true,
                        label = { Text(stringResource(R.string.database)) },
                        trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = picking) },
                        modifier = Modifier.menuAnchor(MenuAnchorType.PrimaryNotEditable).fillMaxWidth(),
                    )
                    ExposedDropdownMenu(expanded = picking, onDismissRequest = { picking = false }) {
                        for (ref in databases) {
                            DropdownMenuItem(text = { Text(vm.databaseLabel(ref)) }, onClick = {
                                database = ref
                                picking = false
                            })
                        }
                    }
                }
            }
        },
        confirmButton = {
            TextButton(enabled = database != null, onClick = {
                database?.let { ref ->
                    vm.saveGame(h.copy(white = white.trim(), black = black.trim(), event = event.trim(), site = site.trim(),
                        date = date.trim(), round = round.trim(), result = result), ref)
                }
                onDismiss()
            }) { Text(stringResource(R.string.save)) }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text(stringResource(android.R.string.cancel)) } },
    )
}

@Composable
fun NewDatabaseDialog(onCreate: (String) -> Unit, onDismiss: () -> Unit) {
    var name by remember { mutableStateOf("") }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(R.string.new_database)) },
        text = {
            OutlinedTextField(name, { name = it }, label = { Text(stringResource(R.string.database_name)) }, singleLine = true,
                keyboardOptions = KeyboardOptions(capitalization = KeyboardCapitalization.Sentences))
        },
        confirmButton = {
            TextButton(enabled = name.isNotBlank(), onClick = {
                onCreate(name)
                onDismiss()
            }) { Text(stringResource(R.string.create)) }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text(stringResource(android.R.string.cancel)) } },
    )
}

@Composable
fun ConfirmDialog(title: String, text: String, confirm: String, onConfirm: () -> Unit, onDismiss: () -> Unit) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(title) },
        text = { Text(text) },
        confirmButton = {
            TextButton(onClick = {
                onConfirm()
                onDismiss()
            }) { Text(confirm) }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text(stringResource(android.R.string.cancel)) } },
    )
}
