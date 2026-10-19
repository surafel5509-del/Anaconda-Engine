package com.anaconda.editor.ui.panels

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.anaconda.editor.EditorViewModel

@Composable
fun ScriptPanel(vm: EditorViewModel, selectedId: Int?) {
    var path by remember { mutableStateOf("scripts/player.lua") }
    var source by remember { mutableStateOf("function onUpdate(dt)\n  -- log(dt)\nend") }
    var err by remember { mutableStateOf("") }
    val lastErr by vm.lastScriptError.collectAsState()
    Card(Modifier.fillMaxWidth().padding(8.dp)) {
        Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Lua Scripting", style = MaterialTheme.typography.titleSmall)
            OutlinedTextField(value = path, onValueChange = { path = it }, label = { Text("Asset path") }, modifier = Modifier.fillMaxWidth())
            OutlinedTextField(value = source, onValueChange = { source = it }, label = { Text("Source") }, modifier = Modifier.fillMaxWidth().height(120.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(onClick = {
                    err = vm.loadScript(path, source)
                    if (err.isEmpty() && selectedId != null) err = vm.bindScript(selectedId, path)
                }) { Text("Bind") }
                Button(enabled = selectedId != null, onClick = { err = vm.hotReloadScript(selectedId!!, source) }) { Text("Hot Reload") }
            }
            if (err.isNotEmpty()) Text("Status: $err", color = MaterialTheme.colorScheme.error, style = MaterialTheme.typography.bodySmall)
            if (lastErr.isNotEmpty()) Text("Engine: $lastErr", style = MaterialTheme.typography.bodySmall)
            Text("Sandbox: io/os/debug blocked, print→log, 1k ops hook.", style = MaterialTheme.typography.bodySmall)
        }
    }
}
