<?php
// Pfade definieren
$config_file = "/usr/local/antswitch-cli/antswitch-cli.config";
$frq_file    = "/usr/local/antswitch-cli/antswitch-cli.frq-map";
$ports_file  = "/usr/local/antswitch-cli/antswitch-cli.ports";
$cli_script  = "/usr/local/antswitch-cli/antswitch-cli.sh";

// --- 1. ADMIN-CHECK ---
$admin_users_line = shell_exec("grep 'ADMIN_USERS=' $config_file");
preg_match('/"(.*)"/', $admin_users_line, $matches);
$allowed_admins = isset($matches[1]) ? explode(' ', $matches[1]) : [];
$current_user = $_SERVER['PHP_AUTH_USER'] ?? $_SERVER['REMOTE_USER'] ?? 'unknown';

if (!in_array($current_user, $allowed_admins)) {
    die("<html><body style='font-family:sans-serif; text-align:center; padding-top:50px;'>
         <h2>Zugriff verweigert</h2><p>Hallo $current_user, du bist nicht als Admin gelistet.</p>
         </body></html>");
}

// --- 2. LABELS LADEN (für die Dropdown-Menüs) ---
$labels = [];
if (file_exists($ports_file)) {
    $lines = file($ports_file, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    foreach ($lines as $line) {
        $line = trim($line);
        if (empty($line) || $line[0] == '#' || $line[0] == ';' || $line[0] == '[') continue;
        if (strpos($line, '=') !== false) {
            list($key, $value) = explode('=', $line, 2);
            $labels[trim($key)] = trim(trim($value), '"');
        }
    }
}

// --- 3. SPEICHER-LOGIK ---
$save_msg = "";
if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_POST['save_frq'])) {
    $content = "# Frequencies for automatic selection\n";
    $content .= "# Format: Start_Hz-End_Hz=Antenna_Output|Tuner\n";
    
    if (isset($_POST['f_start']) && is_array($_POST['f_start'])) {
        foreach ($_POST['f_start'] as $i => $start) {
            $start = trim($start);
            $end   = trim($_POST['f_end'][$i]);
            $ant   = trim($_POST['f_ant'][$i]);
            $tuner = trim($_POST['f_tuner'][$i]);
            
            if ($start !== "" && $end !== "") {
                $content .= "{$start}-{$end}={$ant}|{$tuner}\n";
            }
        }
    }

    if (file_put_contents($frq_file, $content) !== false) {
        $save_msg = "<div style='color:green; padding:10px; border:1px solid green; background:#eaffea; margin-bottom:20px;'>Frequenzkarte erfolgreich gespeichert!</div>";
    } else {
        $save_msg = "<div style='color:red; padding:10px; border:1px solid red; background:#ffeaea; margin-bottom:20px;'>FEHLER: Konnte Datei nicht schreiben. (Berechtigungen?)</div>";
    }
}

// --- 4. SYNC-LOGIK ---
if (isset($_POST['sync_labels'])) {
    $sync_out = shell_exec("$cli_script --sync 2>&1");
    $save_msg = "<div style='color:blue; padding:10px; border:1px solid blue; background:#eaf4ff; margin-bottom:20px;'>Sync ausgeführt: $sync_out</div>";
}

// --- 5. DATEN LADEN (für die Tabelle) ---
$frq_data = [];
if (file_exists($frq_file)) {
    $lines = file($frq_file, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    foreach ($lines as $line) {
        if ($line[0] == '#' || $line[0] == ';') continue;
        if (preg_match('/(\d+)-(\d+)=(\d+)\|(.*)/', $line, $m)) {
            $frq_data[] = ['start' => $m[1], 'end' => $m[2], 'ant' => $m[3], 'tuner' => $m[4]];
        }
    }
}
?>
<!DOCTYPE html>
<html lang="de">
<head>
    <meta charset="UTF-8">
    <title>Antennenschalter Admin</title>
    <style>
        body { font-family: sans-serif; margin: 20px; background: #f4f4f4; color: #333; }
        .box { background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); margin-bottom: 25px; }
        h3 { margin-top: 0; border-bottom: 2px solid #eee; padding-bottom: 10px; color: #0056b3; }
        table { width: 100%; border-collapse: collapse; margin-bottom: 15px; }
        th { text-align: left; font-size: 11px; text-transform: uppercase; color: #777; padding: 8px 5px; }
        td { padding: 4px; border-bottom: 1px solid #f9f9f9; }
        input[type="number"] { width: 100%; padding: 6px; border: 1px solid #ccc; border-radius: 4px; }
        select { width: 100%; padding: 6px; border: 1px solid #ccc; border-radius: 4px; }
        .btn { padding: 10px 20px; cursor: pointer; font-weight: bold; border-radius: 4px; border: none; font-size: 14px; }
        .btn-save { background: #28a745; color: white; }
        .btn-save:hover { background: #218838; }
        .btn-sync { background: #007bff; color: white; }
        iframe { width: 100%; border: 1px solid #ddd; border-radius: 4px; }
        .hw-link { display: inline-block; padding: 15px; background: #fff; border: 1px dashed #007bff; color: #007bff; text-decoration: none; font-weight: bold; border-radius: 4px; }
        .hw-link:hover { background: #007bff; color: #fff; }
    </style>
</head>
<body>

    <h2>AntSwitch Administration <span style="font-weight:normal; font-size: 0.6em;">(User: <?php echo htmlspecialchars($current_user); ?>)</span></h2>
    
    <?php echo $save_msg; ?>

    <div class="box">
        <h3>Live-Ansicht</h3>
        <iframe src="https://india04.ddns.net:52043/antswitch.php" height="110"></iframe>
    </div>

    <div class="box">
        <h3>Frequenz- & Tuner-Konfiguration f&uuml;r UHRR</h3>
        <p style="font-size: 0.85em; color: #666; margin-bottom: 15px;">Definiere hier, bei welcher Frequenz der Antennenschalter umschalten soll und welcher Tuner-Modus aktiv ist.</p>
        <form method="POST">
            <table>
                <tr>
                    <th>Start Freq (Hz)</th>
                    <th>Ende Freq (Hz)</th>
                    <th>Antenne</th>
                    <th>Tuner-Einstellung</th>
                </tr>
                <?php for ($i = 0; $i < 12; $i++): 
                    $d = $frq_data[$i] ?? ['start' => '', 'end' => '', 'ant' => '1', 'tuner' => 'Aus'];
                ?>
                <tr>
                    <td><input type="number" name="f_start[]" value="<?php echo htmlspecialchars($d['start']); ?>"></td>
                    <td><input type="number" name="f_end[]" value="<?php echo htmlspecialchars($d['end']); ?>"></td>
                    <td>
                        <select name="f_ant[]">
                            <?php 
                            $max_out = isset($labels['Outputs']) ? (int)$labels['Outputs'] : 5;
                            for($a=1; $a<=$max_out; $a++) {
                                $btn_label = isset($labels["Out_$a"]) ? $labels["Out_$a"] : "Port $a";
                                if (strtolower($btn_label) == "unbenutzt") continue;
                                $sel = ($d['ant'] == $a) ? 'selected' : '';
                                echo "<option value='$a' $sel>$a: " . htmlspecialchars($btn_label) . "</option>";
                            } ?>
                        </select>
                    </td>
                    <td>
                        <select name="f_tuner[]">
                            <option value="Intern" <?php echo ($d['tuner']=='Intern'?'selected':''); ?>>Interner Tuner</option>
                            <option value="Extern" <?php echo ($d['tuner']=='Extern'?'selected':''); ?>>Externer Tuner</option>
                            <option value="Aus" <?php echo ($d['tuner']=='Aus'?'selected':''); ?>>Tuner AUS</option>
                        </select>
                    </td>
                </tr>
                <?php endfor; ?>
            </table>
            <button type="submit" name="save_frq" class="btn btn-save">💾 Frequenzkarte speichern</button>
        </form>
    </div>

    <div class="box">
        <h3>Hardware-Konfiguration "AntSwitch Remote"</h3>
        <p style="font-size: 0.85em; color: #666;">Konfiguration der Labels und Default-Ports auf dem Antennenschalter <b>(admin:antswitch)</b></p>
        <a href="http://india04.ddns.net:52030/config" target="_blank" class="hw-link">🌐 AntSwitch-Hardware-Config öffnen</a>
        <form method="POST">
        <h4>Anschlie&szlig;end Labels aus Hardware in das System einlesen:</h4>  
          <button type="submit" name="sync_labels" class="btn btn-sync">🔄 Labels synchronisieren</button>
        </form>
    </div>

</body>
</html>


