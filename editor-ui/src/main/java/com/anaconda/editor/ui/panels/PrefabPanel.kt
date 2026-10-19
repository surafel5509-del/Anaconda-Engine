package com.anaconda.editor.ui.panels

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.anaconda.editor.EditorViewModel

@Composable
fun PrefabPanel(vm: EditorViewModel, selectedId: Int?) {
    var status by remember { mutableStateOf("") }
    Card(Modifier.fillMaxWidth().padding(8.dp)) {
        Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Prefabs", style = MaterialTheme.typography.titleSmall)
            Text("Selected: ${selectedId?.toString() ?: "none"}", style = MaterialTheme.typography.bodySmall)
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(enabled = selectedId != null, onClick = {
                    val err = vm.savePrefab(selectedId!!, "/tmp/prefab_${selectedId}.prefab")
                    status = if (err.isEmpty()) "Saved" else "Error: $err"
                }) { Text("Save") }
                Button(onClick = {
                    val id = vm.instantiatePrefab("/tmp/prefab_${selectedId ?: 0}.prefab")
                    status = if (id >= 0) "Instantiated $id" else "Not found"
                }) { Text("Load") }
            }
            if (status.isNotEmpty()) Text(status, style = MaterialTheme.typography.bodySmall)
        }
    }
}
