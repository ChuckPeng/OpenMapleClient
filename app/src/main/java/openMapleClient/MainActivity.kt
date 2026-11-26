package openMapleClient

import android.content.Intent
import android.os.Bundle
import android.util.Log
import android.util.TypedValue
import android.view.WindowManager
import android.view.inputmethod.InputMethodManager
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import org.json.JSONObject
import java.io.BufferedInputStream
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicReference

class MainActivity : AppCompatActivity() {
    private data class NxManifestEntry(
        val name: String,
        val size: Long,
    )

    private val syncCancelled = AtomicBoolean(false)
    private val activeConnection = AtomicReference<HttpURLConnection?>(null)
    private var activeDialog: AlertDialog? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        showTextBoxDialog()
    }

    override fun onDestroy() {
        syncCancelled.set(true)
        disconnectActiveConnectionAsync()
        activeDialog?.dismiss()
        activeDialog = null
        super.onDestroy()
        finish()
    }

    private fun showTextBoxDialog() {
        val savedIp = readSavedHostIp()
        val editText = EditText(this)
        if (savedIp.isNotEmpty()) {
            editText.hint = savedIp
        }

        val dialog = AlertDialog.Builder(this)
            .setTitle("Specify HOST IP Address")
            .setView(editText)
            .setPositiveButton("OK", null)
            .setNegativeButton("Cancel") { d, _ ->
                d.dismiss()
                finish()
            }
            .create()

        activeDialog = dialog
        dialog.show()
        dialog.window?.setSoftInputMode(
            WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN
        )

        dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener {
            val raw = editText.text.toString().trim().ifEmpty { savedIp }
            val (hostIp, nxPort) = parseHostAndPort(raw)
            if (hostIp.isEmpty()) {
                editText.error = "Enter host IP"
                return@setOnClickListener
            }

            hideKeyboard(editText)
            dialog.dismiss()
            if (activeDialog === dialog) {
                activeDialog = null
            }
            saveUserInputToFile(hostIp)
            ensureNxFilesAndStart(hostIp, nxPort)
        }
    }

    private fun parseHostAndPort(raw: String): Pair<String, Int> {
        val trimmed = raw.trim()
        if (trimmed.isEmpty()) {
            return "" to DEFAULT_NX_PORT
        }
        val colonIdx = trimmed.indexOf(':')
        if (colonIdx < 0) {
            return trimmed to DEFAULT_NX_PORT
        }
        val host = trimmed.substring(0, colonIdx).trim()
        val port = trimmed.substring(colonIdx + 1).trim().toIntOrNull()
            ?.takeIf { it in 1..65535 } ?: DEFAULT_NX_PORT
        return host to port
    }

    private fun ensureNxFilesAndStart(hostIp: String, nxPort: Int) {
        val externalDir = getExternalFilesDir(null)
        if (externalDir == null) {
            showSyncErrorDialog(hostIp, nxPort, "External storage directory unavailable")
            return
        }

        if (hasCompleteLocalNxFiles(externalDir)) {
            Log.i(TAG, "Fast-path OK: all ${REQUIRED_NX_FILES.size} .nx files present, launching game")
            startOpenMapleClient()
            return
        }

        syncCancelled.set(false)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        val pad = dpToPx(20)
        val container = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(pad, pad / 2, pad, pad / 2)
        }
        val statusText = TextView(this).apply {
            text = "Connecting to $hostIp:$nxPort..."
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 15f)
        }
        val progressBar = ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal).apply {
            max = 1000
            progress = 0
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                topMargin = dpToPx(12)
                bottomMargin = dpToPx(8)
            }
        }
        val detailText = TextView(this).apply {
            text = "Checking manifest..."
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
        }
        container.addView(statusText)
        container.addView(progressBar)
        container.addView(detailText)

        val progressDialog = AlertDialog.Builder(this)
            .setTitle("Syncing Game Resources")
            .setView(container)
            .setCancelable(false)
            .setNegativeButton("Cancel") { d, _ ->
                syncCancelled.set(true)
                disconnectActiveConnectionAsync()
                window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                d.dismiss()
                finish()
            }
            .create()

        activeDialog = progressDialog
        progressDialog.show()

        Thread({
            try {
                syncNxFiles(externalDir, hostIp, nxPort, statusText, progressBar, detailText)
                if (syncCancelled.get() || isFinishing || isDestroyed) {
                    return@Thread
                }
                runOnUiThread {
                    window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                    progressDialog.dismiss()
                    if (activeDialog === progressDialog) {
                        activeDialog = null
                    }
                    startOpenMapleClient()
                }
            } catch (e: Exception) {
                if (syncCancelled.get() || isFinishing || isDestroyed) {
                    return@Thread
                }
                Log.e(TAG, "Sync failed", e)
                runOnUiThread {
                    window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                    progressDialog.dismiss()
                    if (activeDialog === progressDialog) {
                        activeDialog = null
                    }
                    showSyncErrorDialog(
                        hostIp,
                        nxPort,
                        e.message ?: e.javaClass.simpleName
                    )
                }
            }
        }, "nx-sync").start()
    }

    private fun hasCompleteLocalNxFiles(externalDir: File): Boolean {
        val children = externalDir.listFiles() ?: return false
        if (children.any { it.isFile && it.name.endsWith(".nx.part") }) {
            return false
        }

        val manifestFile = File(externalDir, MANIFEST_FILENAME)
        if (manifestFile.exists()) {
            return try {
                val entries = parseManifest(manifestFile.readText())
                val sizeByName = entries.associate { it.name to it.size }
                REQUIRED_NX_FILES.all { name ->
                    val expected = sizeByName[name] ?: return@all false
                    val file = File(externalDir, name)
                    expected > 0L && file.isFile && file.length() == expected
                }
            } catch (e: Exception) {
                Log.w(TAG, "Failed to read local manifest", e)
                false
            }
        }

        return REQUIRED_NX_FILES.all { name ->
            val file = File(externalDir, name)
            file.isFile && file.length() > 0L
        }
    }

    private fun syncNxFiles(
        externalDir: File,
        hostIp: String,
        nxPort: Int,
        statusText: TextView,
        progressBar: ProgressBar,
        detailText: TextView,
    ) {
        val manifestUrl = "http://$hostIp:$nxPort/manifest.json"
        Log.i(TAG, "Fetching manifest from $manifestUrl")
        val manifestJson = fetchText(manifestUrl)
        val entries = parseManifest(manifestJson)
        val entryByName = entries.associateBy { it.name }

        val missingInManifest = REQUIRED_NX_FILES.filter { !entryByName.containsKey(it) }
        if (missingInManifest.isNotEmpty()) {
            throw IOException("Server manifest missing: ${missingInManifest.joinToString(", ")}")
        }

        val requiredEntries = REQUIRED_NX_FILES.map { entryByName.getValue(it) }
        val pending = requiredEntries.filter { entry ->
            val target = File(externalDir, entry.name)
            !target.isFile || target.length() != entry.size
        }

        if (pending.isEmpty()) {
            File(externalDir, MANIFEST_FILENAME).writeText(manifestJson)
            Log.i(TAG, "All ${requiredEntries.size} files already match manifest")
            return
        }

        val remainingBytes = pending.sumOf { entry ->
            val part = File(externalDir, "${entry.name}.part")
            val partial = if (part.isFile && part.length() in 1L until entry.size) part.length() else 0L
            entry.size - partial
        }
        val neededWithHeadroom = remainingBytes + STORAGE_HEADROOM_BYTES
        if (externalDir.usableSpace < neededWithHeadroom) {
            throw IOException(
                "Need ${formatMiB(neededWithHeadroom)} free space, only ${formatMiB(externalDir.usableSpace)} available"
            )
        }

        val totalPendingBytes = pending.sumOf { it.size }.coerceAtLeast(1L)
        var completedFilesBytes = 0L
        val sessionStartMs = System.currentTimeMillis()
        var sessionNetworkBytes = 0L
        var lastUiUpdateMs = 0L

        for ((index, entry) in pending.withIndex()) {
            if (syncCancelled.get()) {
                throw IOException("Sync cancelled")
            }
            val fileUrl = "http://$hostIp:$nxPort/${entry.name}"
            val downloadedForFile = downloadSingleFileWithResume(
                fileUrl = fileUrl,
                externalDir = externalDir,
                entry = entry,
            ) { currentFilePos, netDelta ->
                sessionNetworkBytes += netDelta
                val now = System.currentTimeMillis()
                if (now - lastUiUpdateMs >= 200L || currentFilePos >= entry.size) {
                    lastUiUpdateMs = now
                    val overallDone = (completedFilesBytes + currentFilePos).coerceAtMost(totalPendingBytes)
                    val overallPermille = ((overallDone * 1000L) / totalPendingBytes).toInt().coerceIn(0, 1000)
                    val overallPct = overallPermille / 10.0
                    val elapsedSec = ((now - sessionStartMs).coerceAtLeast(1L)) / 1000.0
                    val speedMiBs = sessionNetworkBytes / (1024.0 * 1024.0) / elapsedSec
                    val statusMsg = "[${index + 1}/${pending.size}] ${entry.name}"
                    val detailMsg = "${formatMiB(currentFilePos)} / ${formatMiB(entry.size)}  |  " +
                        String.format("%.1f%% (%.1f MB/s)", overallPct, speedMiBs)
                    runOnUiThread {
                        if (!isFinishing && !isDestroyed) {
                            statusText.text = statusMsg
                            progressBar.progress = overallPermille
                            detailText.text = detailMsg
                        }
                    }
                }
            }
            completedFilesBytes += downloadedForFile
        }

        File(externalDir, MANIFEST_FILENAME).writeText(manifestJson)
        Log.i(TAG, "Sync complete: ${pending.size} files updated, manifest saved")
    }

    private fun downloadSingleFileWithResume(
        fileUrl: String,
        externalDir: File,
        entry: NxManifestEntry,
        onProgress: (currentFileBytes: Long, networkDeltaBytes: Long) -> Unit,
    ): Long {
        val targetFile = File(externalDir, entry.name)
        val partFile = File(externalDir, "${entry.name}.part")
        var lastError: IOException? = null

        for (attempt in 1..MAX_FILE_RETRIES) {
            if (syncCancelled.get()) {
                throw IOException("Sync cancelled")
            }
            var conn: HttpURLConnection? = null
            try {
                var existing = if (partFile.isFile) partFile.length() else 0L
                if (existing > entry.size) {
                    partFile.delete()
                    existing = 0L
                }

                if (existing < entry.size) {
                    conn = (URL(fileUrl).openConnection() as HttpURLConnection).apply {
                        connectTimeout = CONNECT_TIMEOUT_MS
                        readTimeout = READ_TIMEOUT_MS
                        instanceFollowRedirects = true
                        if (existing > 0L) {
                            setRequestProperty("Range", "bytes=$existing-")
                        }
                    }
                    activeConnection.set(conn)
                    conn.connect()

                    val code = conn.responseCode
                    if (code == 416) {
                        Log.w(TAG, "HTTP 416 for ${entry.name}, resetting .part")
                        partFile.delete()
                        continue
                    }

                    val appendMode: Boolean
                    val startOffset: Long
                    when {
                        existing > 0L && code == HttpURLConnection.HTTP_PARTIAL -> {
                            appendMode = true
                            startOffset = existing
                            Log.i(TAG, "Resuming ${entry.name} from $startOffset / ${entry.size} (HTTP 206)")
                        }
                        code == HttpURLConnection.HTTP_OK -> {
                            appendMode = false
                            startOffset = 0L
                            Log.i(TAG, "Downloading ${entry.name} (${entry.size} bytes, HTTP 200)")
                        }
                        else -> {
                            throw IOException("HTTP $code for ${entry.name}")
                        }
                    }

                    onProgress(startOffset, 0L)
                    var currentPos = startOffset
                    val buffer = ByteArray(DOWNLOAD_BUFFER_SIZE)
                    BufferedInputStream(conn.inputStream, DOWNLOAD_BUFFER_SIZE).use { input ->
                        FileOutputStream(partFile, appendMode).use { output ->
                            while (true) {
                                if (syncCancelled.get()) {
                                    throw IOException("Sync cancelled")
                                }
                                val read = input.read(buffer)
                                if (read < 0) {
                                    break
                                }
                                output.write(buffer, 0, read)
                                currentPos += read
                                onProgress(currentPos, read.toLong())
                            }
                            output.flush()
                        }
                    }
                }

                val finalLength = partFile.length()
                if (finalLength != entry.size) {
                    throw IOException(
                        "Size mismatch for ${entry.name}: got $finalLength, expected ${entry.size}"
                    )
                }

                if (targetFile.exists() && !targetFile.delete()) {
                    throw IOException("Cannot replace old ${entry.name}")
                }
                if (!partFile.renameTo(targetFile)) {
                    throw IOException("Failed to rename ${partFile.name} to ${entry.name}")
                }
                Log.i(TAG, "Completed ${entry.name} (${entry.size} bytes)")
                return entry.size
            } catch (e: IOException) {
                lastError = e
                Log.w(TAG, "Attempt $attempt/$MAX_FILE_RETRIES failed for ${entry.name}: ${e.message}")
                if (attempt < MAX_FILE_RETRIES && !syncCancelled.get()) {
                    try {
                        Thread.sleep(RETRY_DELAY_MS)
                    } catch (_: InterruptedException) {
                        throw IOException("Sync interrupted")
                    }
                }
            } finally {
                if (conn != null) {
                    activeConnection.compareAndSet(conn, null)
                    conn.disconnect()
                }
            }
        }

        throw lastError ?: IOException("Failed to download ${entry.name}")
    }

    private fun fetchText(urlString: String): String {
        var conn: HttpURLConnection? = null
        return try {
            conn = (URL(urlString).openConnection() as HttpURLConnection).apply {
                connectTimeout = CONNECT_TIMEOUT_MS
                readTimeout = READ_TIMEOUT_MS
                instanceFollowRedirects = true
            }
            activeConnection.set(conn)
            conn.connect()
            val code = conn.responseCode
            if (code != HttpURLConnection.HTTP_OK) {
                throw IOException("HTTP $code from $urlString")
            }
            conn.inputStream.bufferedReader().use { it.readText() }
        } finally {
            if (conn != null) {
                activeConnection.compareAndSet(conn, null)
                conn.disconnect()
            }
        }
    }

    private fun parseManifest(jsonText: String): List<NxManifestEntry> {
        val root = JSONObject(jsonText)
        val array = root.getJSONArray("files")
        val list = ArrayList<NxManifestEntry>(array.length())
        for (i in 0 until array.length()) {
            val obj = array.getJSONObject(i)
            list.add(
                NxManifestEntry(
                    name = obj.getString("name"),
                    size = obj.getLong("size"),
                )
            )
        }
        return list
    }

    private fun showSyncErrorDialog(hostIp: String, nxPort: Int, message: String) {
        if (isFinishing || isDestroyed) {
            return
        }
        val dialog = AlertDialog.Builder(this)
            .setTitle("Resource Sync Failed")
            .setMessage("Could not sync NX files from $hostIp:$nxPort:\n\n$message")
            .setCancelable(false)
            .setPositiveButton("Retry") { d, _ ->
                d.dismiss()
                if (activeDialog === d) {
                    activeDialog = null
                }
                ensureNxFilesAndStart(hostIp, nxPort)
            }
            .setNeutralButton("Change IP") { d, _ ->
                d.dismiss()
                if (activeDialog === d) {
                    activeDialog = null
                }
                showTextBoxDialog()
            }
            .setNegativeButton("Exit") { d, _ ->
                d.dismiss()
                finish()
            }
            .create()
        activeDialog = dialog
        dialog.show()
    }

    private fun disconnectActiveConnectionAsync() {
        val conn = activeConnection.getAndSet(null) ?: return
        Thread {
            try {
                conn.disconnect()
            } catch (_: Exception) {
            }
        }.start()
    }

    private fun startOpenMapleClient() {
        val intent = Intent(this, MapleActivity::class.java)
        startActivity(intent)
        finish()
    }

    private fun hideKeyboard(view: EditText) {
        val imm = getSystemService(INPUT_METHOD_SERVICE) as InputMethodManager
        imm.hideSoftInputFromWindow(view.windowToken, 0)
    }

    private fun readSavedHostIp(): String {
        return try {
            val externalDir = getExternalFilesDir(null) ?: return ""
            val file = File(externalDir, "hostip.txt")
            if (file.isFile) file.readText().trim() else ""
        } catch (_: IOException) {
            ""
        }
    }

    private fun saveUserInputToFile(userInput: String) {
        try {
            val externalDir = getExternalFilesDir(null)
            val file = File(externalDir, "hostip.txt")
            file.writeText(userInput)
        } catch (e: IOException) {
            e.printStackTrace()
        }
    }

    private fun dpToPx(dp: Int): Int {
        return TypedValue.applyDimension(
            TypedValue.COMPLEX_UNIT_DIP,
            dp.toFloat(),
            resources.displayMetrics
        ).toInt()
    }

    private fun formatMiB(bytes: Long): String {
        return String.format("%.1f MB", bytes / (1024.0 * 1024.0))
    }

    companion object {
        private const val TAG = "NxSync"
        private const val DEFAULT_NX_PORT = 8080
        private const val MANIFEST_FILENAME = "nx_manifest.json"
        private const val CONNECT_TIMEOUT_MS = 5000
        private const val READ_TIMEOUT_MS = 15000
        private const val RETRY_DELAY_MS = 500L
        private const val MAX_FILE_RETRIES = 3
        private const val DOWNLOAD_BUFFER_SIZE = 256 * 1024
        private const val STORAGE_HEADROOM_BYTES = 64L * 1024 * 1024

        private val REQUIRED_NX_FILES = listOf(
            "Base.nx",
            "Character.nx",
            "Effect.nx",
            "Etc.nx",
            "Item.nx",
            "Map.nx",
            "MapPretty.nx",
            "MapLatest.nx",
            "Map001.nx",
            "Mob.nx",
            "Morph.nx",
            "Npc.nx",
            "Quest.nx",
            "Reactor.nx",
            "Skill.nx",
            "Sound.nx",
            "String.nx",
            "TamingMob.nx",
            "UI.nx",
        )
    }
}
