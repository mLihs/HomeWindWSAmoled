# HR- und CSC-States sowie übergebene Werte (HomeWindWSAmoled)

Übersicht der setzbaren States und Parameter für HR- und CSC-Widgets.

---

## HR (Heart Rate)

### States (`hr_state_t`)

| State | Bedeutung | Anzeige |
|-------|-----------|---------|
| `HR_STATE_INACTIVE` (0) | Kein HR-Sensor konfiguriert | "Not Configurated" |
| `HR_STATE_ERROR` | Sensor nicht verbunden / Fehler | Name + "Not Connected" |
| `HR_STATE_ACTIVE` | Sensor verbunden, liefert Werte | Name + Wert |

### Funktion: `homewind_set_hr(state, sensor_name, value)`

| Parameter | Typ | Hinweise |
|-----------|-----|----------|
| **state** | `hr_state_t` | `HR_STATE_INACTIVE`, `HR_STATE_ERROR`, `HR_STATE_ACTIVE` |
| **sensor_name** | `const char*` | Beliebig, **max. 31 Zeichen**; `NULL` = aktueller Name bleibt |
| **value** | `uint16_t` | BPM; **0** = Wert bleibt unverändert |

### Weitere HR-Funktionen

- `homewind_set_hr_state(state)` – nur State setzen (Name/Wert unverändert)
- `homewind_set_hr_value(value)` – nur BPM setzen; intern nur bei `value > 0` aktualisiert

### Beispiele

```c
homewind_set_hr(HR_STATE_INACTIVE, NULL, 0);         // Nicht konfiguriert
homewind_set_hr(HR_STATE_ERROR, "TickrFit", 0);      // Nicht verbunden
homewind_set_hr(HR_STATE_ACTIVE, "TickrFit", 129);   // Verbunden + Wert
homewind_set_hr_value(135);                          // Nur Wert aktualisieren
```

---

## CSC (Cadence / Trittfrequenz)

### States (`csc_state_t`)

| State | Bedeutung | Anzeige |
|-------|-----------|---------|
| `CSC_STATE_INACTIVE` (0) | Kein CSC-Sensor konfiguriert | "Not Configurated" |
| `CSC_STATE_ERROR` | Sensor nicht verbunden / Fehler | Name + "Not Connected" |
| `CSC_STATE_ACTIVE` | Sensor verbunden, liefert Cadence | Name + Cadence |

### Funktion: `homewind_set_csc(state, sensor_name, cadence)`

| Parameter | Typ | Hinweise |
|-----------|-----|----------|
| **state** | `csc_state_t` | `CSC_STATE_INACTIVE`, `CSC_STATE_ERROR`, `CSC_STATE_ACTIVE` |
| **sensor_name** | `const char*` | Beliebig, **max. 31 Zeichen**; `NULL` = aktueller Name bleibt |
| **cadence** | `uint16_t` | RPM; **0** = Wert bleibt unverändert |

### Weitere CSC-Funktionen

- `homewind_set_csc_state(state)` – nur State setzen (Name/Cadence unverändert)
- `homewind_set_csc_cadence(cadence)` – nur RPM setzen; intern nur bei `cadence > 0` aktualisiert

### Beispiele

```c
homewind_set_csc(CSC_STATE_INACTIVE, NULL, 0);       // Nicht konfiguriert
homewind_set_csc(CSC_STATE_ERROR, "Wahoo", 0);       // Nicht verbunden
homewind_set_csc(CSC_STATE_ACTIVE, "Wahoo", 90);     // Verbunden + Wert
homewind_set_csc_cadence(95);                        // Nur Cadence aktualisieren
```

---

## Powersave-Anzeige

Im Powersave-Modus werden HR/CSC vereinfacht angezeigt:
- **ACTIVE**: Icon farbig + Wert
- **Sonst**: Icon grau + "--"
