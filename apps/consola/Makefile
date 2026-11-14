# GroovePush monorepo — no-Node fallback orchestration.
# Delegates to each app's native toolchain (CMake/Qt for the GUI, PlatformIO for the processor).

.DEFAULT_GOAL := help
GUI_DIR      := apps/gui
PROC_DIR     := apps/processor

.PHONY: help
help: ## Show this help
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | \
		awk 'BEGIN {FS=":.*?## "}; {printf "  \033[36m%-22s\033[0m %s\n", $$1, $$2}'

## ---- GUI (Qt6 / QML — Raspberry Pi 5) ----
.PHONY: gui-build
gui-build: ## Configure + build the Qt GUI (Release)
	cmake -S $(GUI_DIR) -B $(GUI_DIR)/build -DCMAKE_BUILD_TYPE=Release
	cmake --build $(GUI_DIR)/build -j

.PHONY: gui-run
gui-run: gui-build ## Build and run the GUI locally
	$(GUI_DIR)/build/appPushClone

## ---- Processor (PlatformIO — Teensy 4.1 / NeoTrellis M4) ----
.PHONY: proc-build
proc-build: ## Build all default PlatformIO envs
	pio run -d $(PROC_DIR)

.PHONY: proc-build-teensy
proc-build-teensy: ## Build the Teensy 4.1 firmware
	pio run -d $(PROC_DIR) -e teensy41

.PHONY: proc-upload-teensy
proc-upload-teensy: ## Flash the Teensy 4.1 firmware
	pio run -d $(PROC_DIR) -e teensy41 -t upload

.PHONY: proc-build-m4
proc-build-m4: ## Build the NeoTrellis M4 firmware
	pio run -d $(PROC_DIR) -e neotrellis_m4

.PHONY: proc-tests
proc-tests: ## Build the standalone hardware bring-up tests
	pio run -d $(PROC_DIR) -e test_faders_teensy -e test_encoders_teensy

## ---- Aggregate ----
.PHONY: build
build: proc-build gui-build ## Build everything

.PHONY: clean
clean: ## Remove build artifacts
	rm -rf $(GUI_DIR)/build
	rm -rf $(PROC_DIR)/.pio
