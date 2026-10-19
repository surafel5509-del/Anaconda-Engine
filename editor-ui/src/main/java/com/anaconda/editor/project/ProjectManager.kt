// ============================================================================
//  ProjectManager.kt — on-device project & asset folder system (Month 2).
//
//  Layout (app-private storage — zero runtime permissions needed):
//
//    filesDir/projects/<ProjectName>/
//    ├── project.json          manifest: name, format version, entry scene
//    ├── scenes/main.scene     binary Registry dump (native serializer)
//    ├── assets/sprites/       imported PNG/JPEG (+ .meta.json in Phase 2b)
//    ├── assets/audio/         imported sounds (pipeline in Month 2 audio)
//    └── scripts/              Lua scripts (Phase 4)
//
//  Single-project mode this slice: init() ensures a default project and the
//  current handle never goes null after init. A project switcher UI lands
//  with the Month-2 file browser (roadmap).
// ============================================================================
package com.anaconda.editor.project

import android.content.Context
import org.json.JSONObject
import java.io.File
import java.io.IOException

object ProjectManager {

    /** Manifest format — bump when project.json schema changes. */
    const val MANIFEST_VERSION = 1
    const val API_LEVEL = 2

    data class Project(
        val name: String,
        val root: File,
        val manifest: File,
        val sceneFile: File,
        val spritesDir: File,
        val audioDir: File,
        val scriptsDir: File,
    )

    @Volatile
    private var current: Project? = null

    private var projectsRoot: File? = null

    /** Idempotent — called from EditorViewModel init with the app context. */
    @Synchronized
    fun init(context: Context) {
        if (current != null) return
        val root = File(context.applicationContext.filesDir, "projects").apply {
            check(exists() || mkdirs()) { "cannot create $this" }
        }
        projectsRoot = root
        current = ensureProject(defaultProjectName())
    }

    fun currentProject(): Project =
        current ?: error("ProjectManager.init(context) must run before use")

    fun listProjects(): List<String> =
        projectsRoot?.listFiles { f -> f.isDirectory }?.map { it.name }?.sorted()
            ?: emptyList()

    /** Create-if-missing; switches the current project to it. */
    @Synchronized
    fun ensureProject(name: String): Project {
        val root = File(projectsRoot ?: error("init not called"), sanitize(name))
        val scenes = File(root, "scenes").apply { mkdirs() }
        val sprites = File(root, "assets/sprites").apply { mkdirs() }
        val audio = File(root, "assets/audio").apply { mkdirs() }
        val scripts = File(root, "scripts").apply { mkdirs() }

        val manifest = File(root, "project.json")
        if (!manifest.exists()) {
            manifest.writeText(
                buildString {
                    appendLine("{")
                    appendLine("""  "name": ${JSONObject.quote(name)},""")
                    appendLine("""  "manifestVersion": $MANIFEST_VERSION,""")
                    appendLine("""  "apiLevel": $API_LEVEL,""")
                    appendLine("""  "engine": "anaconda",""")
                    appendLine("""  "entryScene": "scenes/main.scene",""")
                    appendLine("""  "createdEpochMs": ${System.currentTimeMillis()}""")
                    appendLine("}")
                }
            )
        }

        val project = Project(
            name = name,
            root = root,
            manifest = manifest,
            sceneFile = File(scenes, "main.scene"),
            spritesDir = sprites,
            audioDir = audio,
            scriptsDir = scripts,
        )
        current = project
        return project
    }

    /** Sprite files present on disk (PNG/JPEG), newest first. */
    fun listSpriteFiles(): List<File> =
        currentProject().spritesDir
            .listFiles { f -> f.isFile && f.extension.lowercase() in setOf("png", "jpg", "jpeg") }
            ?.sortedByDescending { it.lastModified() }
            ?: emptyList()

    /**
     * Copy raw bytes into the sprite folder with a safe, unique name.
     * Returns the destination File, or throws IOException with a
     * user-readable message.
     */
    @Throws(IOException::class)
    fun importSpriteBytes(sourceName: String, bytes: ByteArray): File {
        val dir = currentProject().spritesDir
        if (!dir.exists()) dir.mkdirs()

        var base = sanitize(File(sourceName).nameWithoutExtension.ifBlank { "sprite" })
        val ext = when (File(sourceName).extension.lowercase()) {
            "png" -> "png"
            "jpg", "jpeg" -> "jpeg"
            else -> throw IOException("Unsupported format — import PNG or JPEG")
        }

        var target = File(dir, "$base.$ext")
        var counter = 1
        while (target.exists()) target = File(dir, "$base (${counter++}).$ext")

        target.writeBytes(bytes)
        if (target.length() == 0L) {
            target.delete()
            throw IOException("Imported file is empty")
        }
        return target
    }

    // ---- helpers -----------------------------------------------------------
    private fun defaultProjectName(): String = "My Game"

    /** Lowercase, no path separators, spaces kept (folder-safe). */
    internal fun sanitize(raw: String): String {
        val cleaned = raw.replace(Regex("""[\\/:*?"<>|\x00-\x1F]"""), "_")
            .trim()
            .replace(Regex("""\s+"""), " ")
        return cleaned.ifBlank { "Untitled" }
    }
}
