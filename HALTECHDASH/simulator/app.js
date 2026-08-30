const telemetry = {
  rpm: 3200,
  lambda: 1.0,
  tps: 18,
  map: 54,
  cam: 24,
  ignition: 31,
  injector: 24,
  coolant: 184,
  battery: 13.8,
  iat: 82,
  canOnline: true,
  checkEngine: false
};

const controls = [
  { key: "rpm", label: "RPM", min: 0, max: 9000, step: 50, unit: "" },
  { key: "lambda", label: "Lambda", min: 0.72, max: 1.22, step: 0.001, unit: "" },
  { key: "tps", label: "TPS", min: 0, max: 100, step: 1, unit: "%" },
  { key: "map", label: "MAP", min: 20, max: 105, step: 1, unit: " kPa" },
  { key: "iat", label: "IAT", min: 40, max: 170, step: 1, unit: " F" },
  { key: "ignition", label: "Ignition angle", min: -10, max: 50, step: 1, unit: " deg" },
  { key: "injector", label: "Injector duty", min: 0, max: 100, step: 1, unit: "%" },
  { key: "cam", label: "Intake cam", min: -10, max: 55, step: 1, unit: " deg" },
  { key: "coolant", label: "Coolant", min: 60, max: 250, step: 1, unit: " F" },
  { key: "battery", label: "Battery", min: 10, max: 15.2, step: 0.1, unit: " V" }
];

const presets = {
  idle: { rpm: 950, lambda: 1.0, tps: 2, map: 34, cam: 0, ignition: 14, injector: 2, coolant: 178, battery: 13.9, iat: 76 },
  cruise: { rpm: 3200, lambda: 1.0, tps: 18, map: 54, cam: 24, ignition: 31, injector: 24, coolant: 184, battery: 13.8, iat: 82 },
  pull: { rpm: 7600, lambda: 0.88, tps: 100, map: 96, cam: 42, ignition: 28, injector: 82, coolant: 190, battery: 13.6, iat: 94 },
  hot: { rpm: 4100, lambda: 0.93, tps: 42, map: 86, cam: 36, ignition: 24, injector: 46, coolant: 226, battery: 12.4, iat: 128 }
};

let demoRunning = true;
let lastPreset = "cruise";
let bootStartMs = performance.now();
const bootDurationMs = 1730;
const rangeInputs = new Map();

const $ = (selector) => document.querySelector(selector);
const $$ = (selector) => Array.from(document.querySelectorAll(selector));

function clamp(value, min, max) {
  return Math.min(max, Math.max(min, value));
}

function setArc(path, value, max) {
  const length = path.getTotalLength();
  const ratio = clamp(value / max, 0, 1);
  path.style.strokeDasharray = String(length);
  path.style.strokeDashoffset = String(length * (1 - ratio));
}

function easeInOut(t) {
  const v = clamp(t, 0, 1);
  return v < 0.5 ? 2 * v * v : 1 - Math.pow(-2 * v + 2, 2) / 2;
}

function bootRatio(nowMs) {
  const elapsed = nowMs - bootStartMs;
  const upMs = 850;
  const holdMs = 180;
  const downMs = 700;

  if (elapsed < 0) return 0;
  if (elapsed <= upMs) return easeInOut(elapsed / upMs);
  if (elapsed <= upMs + holdMs) return 1;
  if (elapsed <= bootDurationMs) return 1 - easeInOut((elapsed - upMs - holdMs) / downMs);
  return null;
}

function format(key, value) {
  if (key === "lambda") return value.toFixed(3);
  if (key === "battery") return value.toFixed(1);
  return Math.round(value).toString();
}

function displayOrDash(key, unit = "") {
  if (!telemetry.canOnline) return "--";
  return `${format(key, telemetry[key])}${unit}`;
}

function render() {
  const canDots = $$("[data-can-dot]");
  const canTexts = $$("[data-can-text]");
  canDots.forEach((dot) => dot.classList.toggle("off", !telemetry.canOnline));
  canTexts.forEach((text) => {
    text.textContent = telemetry.canOnline ? "CAN" : "NO CAN";
  });

  $("#rpm-value").textContent = telemetry.canOnline ? Math.round(telemetry.rpm) : "0";
  setArc($("#rpm-arc"), telemetry.canOnline ? telemetry.rpm : 0, 9000);
  $("#rpm-arc").classList.toggle("red", telemetry.rpm >= 8400);
  $("#rpm-arc").classList.toggle("yellow", telemetry.rpm >= 7800 && telemetry.rpm < 8400);

  $("#coolant-left-value").textContent = displayOrDash("coolant", " F");
  $("#tps-value").textContent = displayOrDash("tps", "%");
  $("#map-value").textContent = displayOrDash("map", " kPa");
  $("#iat-left-value").textContent = displayOrDash("iat", " F");
  $("#ignition-value").textContent = displayOrDash("ignition", " deg");

  $("#coolant-main-value").textContent = telemetry.canOnline ? Math.round(telemetry.coolant) : "--";
  setArc($("#coolant-arc"), telemetry.canOnline ? telemetry.coolant : 0, 250);

  $("#coolant-value").textContent = displayOrDash("coolant", " F");
  $("#injector-value").textContent = displayOrDash("injector", "%");
  $("#battery-value").textContent = displayOrDash("battery", " V");
  $("#lambda-value").textContent = displayOrDash("lambda");
  $("#cam-value").textContent = displayOrDash("cam", " deg");

  const warning = $("#warning-value");
  warning.classList.remove("warn-red", "warn-yellow");
  if (!telemetry.canOnline) {
    warning.textContent = "NO CAN DATA";
    warning.style.color = "var(--red)";
  } else if (telemetry.coolant >= 220) {
    warning.textContent = "COOLANT HOT";
    warning.style.color = "var(--red)";
  } else if (telemetry.checkEngine) {
    warning.textContent = "CHECK ENGINE";
    warning.style.color = "var(--yellow)";
  } else if (telemetry.battery < 12) {
    warning.textContent = "LOW BATTERY";
    warning.style.color = "var(--yellow)";
  } else {
    warning.textContent = "SYSTEM OK";
    warning.style.color = "var(--green)";
  }

  for (const control of controls) {
    const input = rangeInputs.get(control.key);
    if (input && document.activeElement !== input) {
      input.value = telemetry[control.key];
      input.previousElementSibling.querySelector("strong").textContent = `${format(control.key, telemetry[control.key])}${control.unit}`;
    }
  }

  $("#can-online").checked = telemetry.canOnline;
  $("#check-engine").checked = telemetry.checkEngine;
}

function buildControls() {
  const host = $("#controls");
  controls.forEach((control) => {
    const wrap = document.createElement("div");
    wrap.className = "control";

    const label = document.createElement("label");
    label.htmlFor = `control-${control.key}`;
    label.innerHTML = `<span>${control.label}</span><strong>${format(control.key, telemetry[control.key])}${control.unit}</strong>`;

    const input = document.createElement("input");
    input.id = `control-${control.key}`;
    input.type = "range";
    input.min = control.min;
    input.max = control.max;
    input.step = control.step;
    input.value = telemetry[control.key];
    input.addEventListener("input", () => {
      demoRunning = false;
      updateDemoButton();
      telemetry[control.key] = Number(input.value);
      label.querySelector("strong").textContent = `${format(control.key, telemetry[control.key])}${control.unit}`;
      render();
    });

    wrap.append(label, input);
    host.append(wrap);
    rangeInputs.set(control.key, input);
  });
}

function applyPreset(name) {
  lastPreset = name;
  Object.assign(telemetry, presets[name]);
  $$(".segmented button").forEach((button) => button.classList.toggle("active", button.dataset.preset === name));
  render();
}

function updateDemoButton() {
  const button = $("#demo-toggle");
  const icon = button.querySelector(".icon");
  icon.dataset.icon = demoRunning ? "pause" : "play";
  button.title = demoRunning ? "Pause demo animation" : "Resume demo animation";
  button.setAttribute("aria-label", button.title);
}

function bindEvents() {
  $$(".segmented button").forEach((button) => {
    button.addEventListener("click", () => {
      demoRunning = false;
      updateDemoButton();
      applyPreset(button.dataset.preset);
    });
  });

  $("#demo-toggle").addEventListener("click", () => {
    demoRunning = !demoRunning;
    updateDemoButton();
  });

  $("#can-online").addEventListener("change", (event) => {
    telemetry.canOnline = event.target.checked;
    render();
  });
  $("#check-engine").addEventListener("change", (event) => {
    telemetry.checkEngine = event.target.checked;
    render();
  });
}

function animate() {
  const now = performance.now();
  const sweep = bootRatio(now);

  if (sweep !== null) {
    telemetry.rpm = 9000 * sweep;
    telemetry.coolant = 250 * sweep;
    render();
  } else if (demoRunning) {
    const seconds = now / 1000;
    const base = presets[lastPreset];
    telemetry.rpm = clamp(base.rpm + Math.sin(seconds * 2.1) * 450 + Math.sin(seconds * 0.7) * 180, 0, 9000);
    telemetry.tps = clamp(base.tps + Math.sin(seconds * 1.4) * 6, 0, 100);
    telemetry.map = clamp(base.map + Math.sin(seconds * 1.8) * 5, 20, 105);
    telemetry.lambda = clamp(base.lambda + Math.sin(seconds * 2.5) * 0.018, 0.72, 1.22);
    telemetry.cam = clamp(base.cam + Math.sin(seconds * 1.1) * 3, -10, 55);
    telemetry.ignition = clamp(base.ignition + Math.sin(seconds * 1.6) * 2, -10, 50);
    telemetry.injector = clamp(base.injector + Math.sin(seconds * 1.9) * 3, 0, 100);
    telemetry.coolant = clamp(base.coolant + Math.sin(seconds * 0.18) * 2, 60, 250);
    telemetry.battery = clamp(base.battery + Math.sin(seconds * 0.8) * 0.1, 10, 15.2);
    telemetry.iat = clamp(base.iat + Math.sin(seconds * 0.35) * 2, 40, 170);
    render();
  }
  requestAnimationFrame(animate);
}

buildControls();
bindEvents();
applyPreset("cruise");
updateDemoButton();
animate();
