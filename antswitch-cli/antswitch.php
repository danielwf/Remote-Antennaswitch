<?php
// Pfade definieren
$cli_script = "/usr/local/antswitch-cli/antswitch-cli.sh";
$ports_file = "/usr/local/antswitch-cli/antswitch-cli.ports";

// --- 1. KONFIGURATION LADEN ---
$labels = []; $defaults = [];
if (file_exists($ports_file)) {
    $lines = file($ports_file, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    foreach ($lines as $line) {
        $line = trim($line);
        if (empty($line) || $line[0] == '#' || $line[0] == ';' || $line[0] == '[') continue;
        if (strpos($line, '=') !== false) {
            list($key, $value) = explode('=', $line, 2);
            $key = trim($key); $value = trim(trim($value), '"'); 
            if (strpos($key, 'In_') === 0 || strpos($key, 'Out_') === 0 || $key == 'Inputs' || $key == 'Outputs') {
                $labels[$key] = $value;
            } else { $defaults[$key] = $value; }
        }
    }
}

// --- 2. AJAX STATUS-LOGIK ---
if (isset($_GET['ajax'])) {
    $ps_check = shell_exec("ps -ax | grep -v grep | grep 'UHRR'");
    $is_locked = !empty($ps_check);
    $current_out = trim(shell_exec("$cli_script --get-out 2>&1"));
    $current_in  = trim(shell_exec("$cli_script --get-in 2>&1"));
    header('Content-Type: application/json');
    echo json_encode(["out" => $current_out, "in" => $current_in, "locked" => $is_locked]);
    exit; 
}

// --- 3. SCHALTLOGIK ---
if (isset($_GET['set_in'])) { shell_exec("$cli_script --set-in " . intval($_GET['set_in'])); header("Location: antswitch.php"); exit; }
if (isset($_GET['set_out'])) { shell_exec("$cli_script --set-out " . intval($_GET['set_out'])); header("Location: antswitch.php"); exit; }
?>
<!DOCTYPE html>
<html lang="de">
<head>
    <meta charset="UTF-8">
    <style>
        body { font-family: sans-serif; background: transparent; margin: 0; padding: 10px; color: #333; }
        .row { display: flex; gap: 10px; align-items: center; margin-bottom: 10px; }
        .label-text { font-size: 13px; font-weight: bold; min-width: 65px; }
        .btn-container { display: flex; gap: 5px; align-items: center; flex-wrap: wrap; }
        .btn { 
            padding: 8px 12px; border: 1px solid #666; background: #eee; 
            cursor: pointer; font-weight: bold; border-radius: 3px; 
            text-decoration: none; color: #333; font-size: 12px; transition: 0.3s;
            white-space: nowrap; line-height: 1.2; display: inline-flex; align-items: center;
            height: 32px; box-sizing: border-box;
        }

        /* 1. Normaler Aktiv-Zustand (Grün) */
        .btn.active { background: #28a745 !important; color: white !important; border-color: #1e7e34; }

        /* 2. Aktivantenne-Zustand (Blau) - !important sorgt dafür, dass Grau überschrieben wird */
        .btn.in3-rx-active { background: #007bff !important; color: white !important; border-color: #0056b3; }

        /* 3. Sperr-Zustand (Grau / Klick-Stop) */
        .btn.disabled { background: #ccc; color: #777; cursor: not-allowed; opacity: 0.6; pointer-events: none; border-color: #bbb; }
        
        /* WICHTIG: Wenn der Button blau UND gesperrt ist, soll er blau bleiben, aber die Deckkraft verlieren */
        .btn.in3-rx-active.disabled { opacity: 0.8; pointer-events: none; }

        .lock-msg { color: #d9534f; font-weight: bold; font-size: 11px; margin-left: 10px; }
        .rx-hint { font-size: 10px; font-weight: normal; margin-left: 5px; opacity: 0.9; }
    </style>
</head>
<body>

<div class="row">
    <div class="label-text">Eingang:</div>
    <div class="btn-container">
        <?php
        $input_count = isset($labels['Inputs']) ? intval($labels['Inputs']) : 3;
        for ($i = 1; $i <= $input_count; $i++) {
            $label = isset($labels["In_$i"]) ? $labels["In_$i"] : "In $i";
            if (strtolower($label) == "unbenutzt") continue;
            echo '<a href="?set_in='.$i.'" id="btn-in-'.$i.'" class="btn">';
            echo '<span>'.htmlspecialchars($label).'</span>';
            if ($i == 3) echo '<span id="rx-status-3" class="rx-hint"></span>';
            echo '</a>';
        }
        ?>
    </div>
</div>

<div class="row">
    <div class="label-text">Ausgang:</div>
    <div class="btn-container">
        <?php
        $output_count = isset($labels['Outputs']) ? intval($labels['Outputs']) : 5;
        for ($i = 1; $i <= $output_count; $i++) {
            $label = isset($labels["Out_$i"]) ? $labels["Out_$i"] : "Out $i";
            if (strtolower($label) == "unbenutzt") continue;
            echo '<a href="?set_out='.$i.'" id="btn-out-'.$i.'" class="btn">'.htmlspecialchars($label).'</a>';
        }
        ?>
        <span id="lock-info" class="lock-msg" style="display:none;">⚠ TX AKTIV</span>
    </div>
</div>

<script>
function updateStatus() {
    fetch('antswitch.php?ajax=1')
        .then(response => response.json())
        .then(data => {
            const lockInfo = document.getElementById('lock-info');
            lockInfo.style.display = data.locked ? 'inline' : 'none';

            document.querySelectorAll('.btn').forEach(btn => {
                const isInputBtn = btn.id.startsWith('btn-in-');
                const portId = btn.id.replace(isInputBtn ? 'btn-in-' : 'btn-out-', '');
                const currentVal = isInputBtn ? data.in : data.out;

                // Reset Sonderstatus für Eingang 3
                if (btn.id === 'btn-in-3') {
                    btn.classList.remove('in3-rx-active');
                    document.getElementById('rx-status-3').innerText = '';
                }

                // Grüne Markierung nur, wenn NICHT gesperrt
                if (currentVal == portId && !data.locked) {
                    btn.classList.add('active');
                } else {
                    btn.classList.remove('active');
                    
                    // Blau-Zustand für Eingang 3 (auch wenn gesperrt!)
                    if (btn.id === 'btn-in-3' && (currentVal != portId || data.locked)) {
                        btn.classList.add('in3-rx-active');
                        document.getElementById('rx-status-3').innerText = 'an Aktivantenne';
                    }
                }

                // Generelle Sperre (Klick-Stop)
                if (data.locked) {
                    btn.classList.add('disabled');
                } else {
                    btn.classList.remove('disabled');
                }
            });
        }).catch(err => console.error('Update failed'));
}
setInterval(updateStatus, 2000);
updateStatus();
</script>
</body>
</html>

