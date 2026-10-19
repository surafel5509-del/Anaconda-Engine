package com.anaconda.editor.ui.panels

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.anaconda.editor.EditorViewModel

@Composable
fun ExportPanel(vm: EditorViewModel) {
    var pkg by remember { mutableStateOf("com.example.mygame") }
    var appName by remember { mutableStateOf("My Game") }
    var ver by remember { mutableStateOf("1.0.0") }
    var status by remember { mutableStateOf("") }
    Card(Modifier.fillMaxWidth().padding(8.dp)) {
        Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Export", style = MaterialTheme.typography.titleSmall)
            OutlinedTextField(value = pkg, onValueChange = { pkg = it }, label = { Text("Package") }, modifier = Modifier.fillMaxWidth())
            OutlinedTextField(value = appName, onValueChange = { appName = it }, label = { Text("App name") }, modifier = Modifier.fillMaxWidth())
            OutlinedTextField(value = ver, onValueChange = { ver = it }, label = { Text("Version") }, modifier = Modifier.fillMaxWidth())
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(onClick = { status = vm.exportProject(pkg, appName, ver) }) { Text("Export APK") }
                Button(onClick = { status = "Pack: ${vm.packProject("/tmp/pack.anaconda")}" }) { Text("Pack") }
            }
            if (status.isNotEmpty()) Text(status, style = MaterialTheme.typography.bodySmall)
            Text("Validates package, reserves com.anaconda, template→re-sign→align (Month 5).", style = MaterialTheme.typography.bodySmall)
        }
    }
}
