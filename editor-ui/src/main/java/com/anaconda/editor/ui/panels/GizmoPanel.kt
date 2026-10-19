package com.anaconda.editor.ui.panels

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.anaconda.editor.EditorViewModel

@Composable
fun GizmoPanel(vm: EditorViewModel) {
    val mode by vm.gizmoMode.collectAsState()
    Card(Modifier.fillMaxWidth().padding(8.dp)) {
        Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Gizmo", style = MaterialTheme.typography.titleSmall)
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                listOf("Translate" to 0, "Rotate" to 1, "Scale" to 2).forEach { (label, m) ->
                    FilterChip(selected = mode == m, onClick = { vm.setGizmoMode(m) }, label = { Text(label) })
                }
            }
            Text("Drag on viewport to move selected entity (snaps if enabled).", style = MaterialTheme.typography.bodySmall)
        }
    }
}
