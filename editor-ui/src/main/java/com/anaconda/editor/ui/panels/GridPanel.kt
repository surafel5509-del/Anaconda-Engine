package com.anaconda.editor.ui.panels

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.anaconda.editor.EditorViewModel

@Composable
fun GridPanel(vm: EditorViewModel) {
    val gridEnabled by vm.gridEnabled.collectAsState()
    val snapEnabled by vm.snapEnabled.collectAsState()
    val cellSize by vm.cellSize.collectAsState()
    Card(Modifier.fillMaxWidth().padding(8.dp)) {
        Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Grid & Snap", style = MaterialTheme.typography.titleSmall)
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                FilterChip(selected = gridEnabled, onClick = { vm.setGridEnabled(!gridEnabled) }, label = { Text("Grid") })
                FilterChip(selected = snapEnabled, onClick = { vm.setSnapEnabled(!snapEnabled) }, label = { Text("Snap") })
            }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Cell: ${"%.1f".format(cellSize)}")
                Slider(value = cellSize, onValueChange = { vm.setCellSize(it) }, valueRange = 0.25f..4f, modifier = Modifier.weight(1f))
            }
        }
    }
}
