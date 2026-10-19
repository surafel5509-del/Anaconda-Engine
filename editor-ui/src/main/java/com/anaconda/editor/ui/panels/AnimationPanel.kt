package com.anaconda.editor.ui.panels

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.anaconda.editor.EditorViewModel

@Composable
fun AnimationPanel(vm: EditorViewModel, selectedId: Int?) {
    var clipName by remember { mutableStateOf("walk") }
    var fps by remember { mutableStateOf("12") }
    var status by remember { mutableStateOf("") }
    Card(Modifier.fillMaxWidth().padding(8.dp)) {
        Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Animator", style = MaterialTheme.typography.titleSmall)
            OutlinedTextField(value = clipName, onValueChange = { clipName = it }, label = { Text("Clip") }, modifier = Modifier.fillMaxWidth())
            OutlinedTextField(value = fps, onValueChange = { fps = it }, label = { Text("FPS") }, modifier = Modifier.fillMaxWidth())
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(onClick = {
                    val f = fps.toFloatOrNull() ?: 12f
                    status = vm.createClip(clipName, f)
                }) { Text("Create Clip") }
                Button(enabled = selectedId != null, onClick = { vm.setAnimatorClip(selectedId!!, clipName); status = "Bound" }) { Text("Bind") }
                Button(enabled = selectedId != null, onClick = { vm.setAnimatorPlaying(selectedId!!, true) }) { Text("Play") }
                Button(enabled = selectedId != null, onClick = { vm.setAnimatorPlaying(selectedId!!, false) }) { Text("Stop") }
            }
            if (status.isNotEmpty()) Text(status, style = MaterialTheme.typography.bodySmall)
        }
    }
}
