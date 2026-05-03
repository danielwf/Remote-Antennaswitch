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

// --- 2. KONFIGURATION / LABELS LADEN ---
$labels = [];
$defaults = [];
if (file_exists($ports_file)) {
    $lines = file($ports_file, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    foreach ($lines as $line) {
        $line = trim($line);
        if (empty($line) || $line[0] == '#' || $line[0] == ';' || $line[0] == '[') continue;
        if (strpos($line, '=') !== false) {
            list($key, $value) = explode('=', $line, 2);
            $key = trim($key);
            $val = trim(trim($value), '"');
            if (strpos($key, 'In_') === 0 || strpos($key, 'Out_') === 0 || $key == 'Inputs' || $key == 'Outputs') {
                $labels[$key] = $val;
            } else {
                $defaults[$key] = $val;
            }
        }
    }
}
// Fallbacks
$max_in = isset($labels['Inputs']) ? (int)$labels['Inputs'] : 3;
$max_out = isset($labels['Outputs']) ? (int)$labels['Outputs'] : 5;

// --- 3. SPEICHER-LOGIK FREQUENZKARTE ---
$save_msg = "";
if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_POST['save_frq'])) {
    $content = "# Frequencies for automatic selection\n# Format: Start_Hz-End_Hz=Antenna_Output|Tuner (int, ext, off)\n";
    if (isset($_POST['f_start']) && is_array($_POST['f_start'])) {
        foreach ($_POST['f_start'] as $i => $start) {
            $start = trim($start);
            if ($start !== "" && isset($_POST['f_end'][$i])) {
                $content .= "{$start}-".trim($_POST['f_end'][$i])."=".trim($_POST['f_ant'][$i])."|".trim($_POST['f_tuner'][$i])."\n";
            }
        }
    }
    if (file_put_contents($frq_file, $content) !== false) {
        $save_msg = "<div style='color:green; padding:10px; border:1px solid green; background:#eaffea; margin-bottom:20px;'>Frequenzkarte erfolgreich gespeichert!</div>";
    }
}

// --- 4. SPEICHER-LOGIK LOKALE LABELS ---
if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_POST['save_labels_local'])) {
    $content = "# Global config of antennas and labels\n[Labels]\n";
    $content .= "Inputs=$max_in\nOutputs=$max_out\n";
    for ($i = 1; $i <= $max_in; $i++) {
        $val = trim($_POST['l_in'][$i] ?? "In $i");
        $content .= "In_$i=\"$val\"\n";
    }
    for ($i = 1; $i <= $max_out; $i++) {
        $val = trim($_POST['l_out'][$i] ?? "Out $i");
        $content .= "Out_$i=\"$val\"\n";
    }
    $content .= "\n[Defaults]\n";
    $content .= "DEFAULT_INPUT=".($defaults['DEFAULT_INPUT'] ?? 1)."\n";
    $content .= "DEFAULT_OUTPUT=".($defaults['DEFAULT_OUTPUT'] ?? 1)."\n";

    if (file_put_contents($ports_file, $content) !== false) {
        header("Location: ".$_SERVER['PHP_SELF']."?msg=labels_saved"); exit;
    }
}

// --- 5. SYNC-LOGIK HARDWARE ---
if (isset($_POST['sync_labels'])) {
    shell_exec("$cli_script --sync 2>&1");
    header("Location: ".$_SERVER['PHP_SELF']."?msg=sync_done"); exit;
}

// Nachrichtensystem
if (isset($_GET['msg'])) {
    if ($_GET['msg'] == 'labels_saved') $save_msg = "<div style='color:green; padding:10px; border:1px solid green; background:#eaffea; margin-bottom:20px;'>Lokale Labels gespeichert!</div>";
    if ($_GET['msg'] == 'sync_done') $save_msg = "<div style='color:blue; padding:10px; border:1px solid blue; background:#eaf4ff; margin-bottom:20px;'>Hardware-Sync abgeschlossen!</div>";
}

// --- 6. DATEN FÜR TABELLE LADEN ---
$frq_data = [];
if (file_exists($frq_file)) {
    $lines = file($frq_file, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    foreach ($lines as $line) {
        if ($line[0] == '#' || $line[0] == ';') continue;
        if (preg_match('/(\d+)-(\d+)=(\d+)\|(.*)/', $line, $m)) {
            $frq_data[] = ['start' => $m[1], 'end' => $m[2], 'ant' => $m[3], 'tuner' => trim($m[4])];
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
        input, select { width: 100%; padding: 6px; border: 1px solid #ccc; border-radius: 4px; box-sizing: border-box; }
        .btn { padding: 10px 20px; cursor: pointer; font-weight: bold; border-radius: 4px; border: none; font-size: 14px; }
        .btn-save { background: #28a745; color: white; }
        .btn-sync { background: #007bff; color: white; }
        iframe { width: 100%; border: 1px solid #ddd; border-radius: 4px; }
        .hw-link { display: inline-block; padding: 15px; background: #fff; border: 1px dashed #007bff; color: #007bff; text-decoration: none; font-weight: bold; border-radius: 4px; }
        .hw-link:hover { background: #007bff; color: #fff; }
        .label-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }
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
        <h3>Frequenz- & Tuner-Konfiguration (UHRR)</h3>
        <form method="POST">
            <table>
                <tr><th>Start Hz</th><th>Ende Hz</th><th>Antenne</th><th>Tuner</th></tr>
                <?php for ($i = 0; $i < 12; $i++): $d = $frq_data[$i] ?? ['start'=>'','end'=>'','ant'=>'1','tuner'=>'off']; ?>
                <tr>
                    <td><input type="number" name="f_start[]" value="<?php echo $d['start']; ?>"></td>
                    <td><input type="number" name="f_end[]" value="<?php echo $d['end']; ?>"></td>
                    <td>
                        <select name="f_ant[]">
                            <?php for($a=1; $a<=$max_out; $a++) {
                                $l = $labels["Out_$a"] ?? "Port $a";
                                if (strtolower($l) == "unbenutzt") continue;
                                echo "<option value='$a' ".($d['ant']==$a?'selected':'').">$a: $l</option>";
                            } ?>
                        </select>
                    </td>
                    <td>
                        <select name="f_tuner[]">
                            <option value="int" <?php echo ($d['tuner']=='int'?'selected':''); ?>>interner Tuner</option>
                            <option value="ext" <?php echo ($d['tuner']=='ext'?'selected':''); ?>>externer Tuner</option>
                            <option value="off" <?php echo ($d['tuner']=='off'?'selected':''); ?>>kein Tuner</option>
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

    <div class="box">
        <h3>Lokale Label-Konfiguration</h3>
        <p style="font-size: 0.85em; color: #666; margin-bottom: 15px;">Lokales &Uuml;berschreiben der Anschlusslabel, ohne Speicherung in der Hardware</p>
        <form method="POST">
            <div class="label-grid">
                <div>
                    <h4>Eingänge (Inputs)</h4>
                    <?php for($i=1; $i<=$max_in; $i++): ?>
                        <div style="margin-bottom:5px;">
                            <label style="font-size:11px;">In <?php echo $i; ?>:</label>
                            <input type="text" name="l_in[<?php echo $i; ?>]" value="<?php echo htmlspecialchars($labels["In_$i"] ?? ""); ?>">
                        </div>
                    <?php endfor; ?>
                </div>
                <div>
                    <h4>Ausgänge (Outputs)</h4>
                    <?php for($i=1; $i<=$max_out; $i++): ?>
                        <div style="margin-bottom:5px;">
                            <label style="font-size:11px;">Out <?php echo $i; ?>:</label>
                            <input type="text" name="l_out[<?php echo $i; ?>]" value="<?php echo htmlspecialchars($labels["Out_$i"] ?? ""); ?>">
                        </div>
                    <?php endfor; ?>
                </div>
            </div>
            <br>
            <button type="submit" name="save_labels_local" class="btn btn-save" style="background:#6c757d;">📝 Lokale Labels speichern</button>
        </form>
    </div>

</body>
</html>
