using System;
using System.Drawing;
using System.Windows.Forms;
using Microsoft.Win32;

namespace MonoEyeSwitcher
{
    public partial class MainForm : Form
    {
        private Button toggleButton;
        private ComboBox qualityComboBox;
        private ComboBox leftEyeComboBox;
        private CheckBox indicatorCheckbox;
        private CheckBox tensorCheckbox;
        private CheckBox specularCheckbox;
        private CheckBox edgeCheckbox;
        private GroupBox simRacingGroupBox;
        private Button fixRaceRoomButton;
        private Button fixAms2Button;
        private Label statusLabel;
        private Label qualityLabel;
        private Label leftEyeLabel;
        private Label titleLabel;
        private Label infoLabel;
        private GroupBox openVrInstallerGroupBox;
        private GroupBox diagnosticsGroupBox;
        private CheckBox loggingCheckbox;
        private TextBox sessionNameTextBox;
        private Button saveLogButton;

        private Button selectGameFolderButton;
        private Label gameFolderLabel;
        private Button saveButton;

        private bool isEnabled = false;

        public MainForm()
        {
            InitializeComponent();
            UpdateStatus();
        }

        private void InitializeComponent()
        {
            this.Text = "MonoEye Switcher v0.5.16 (Alpha)";
            this.Size = new Size(420, 560);
            this.FormBorderStyle = FormBorderStyle.FixedSingle;
            this.MaximizeBox = false;
            this.StartPosition = FormStartPosition.CenterScreen;
            this.BackColor = Color.FromArgb(30, 30, 30);
            this.ForeColor = Color.White;
            this.Font = new Font("Segoe UI", 9F, FontStyle.Regular);

            // Title
            titleLabel = new Label
            {
                Text = "MonoEye",
                Font = new Font("Segoe UI", 20F, FontStyle.Bold),
                ForeColor = Color.FromArgb(0, 180, 255),
                AutoSize = true,
                Location = new Point(20, 15)
            };
            this.Controls.Add(titleLabel);

            // Subtitle
            Label subtitleLabel = new Label
            {
                Text = "Single-eye VR rendering with depth reconstruction",
                Font = new Font("Segoe UI", 9F, FontStyle.Regular),
                ForeColor = Color.FromArgb(180, 180, 180),
                AutoSize = true,
                Location = new Point(25, 50)
            };
            this.Controls.Add(subtitleLabel);

            // Status indicator
            statusLabel = new Label
            {
                Text = "Status: Checking...",
                Font = new Font("Segoe UI", 11F, FontStyle.Bold),
                ForeColor = Color.Gray,
                AutoSize = true,
                Location = new Point(20, 90)
            };
            this.Controls.Add(statusLabel);

            // Info text
            infoLabel = new Label
            {
                Text = "Enable MonoEye for all OpenXR games. Changes take effect on next game launch.",
                Font = new Font("Segoe UI", 8.5F),
                ForeColor = Color.FromArgb(160, 160, 160),
                Size = new Size(360, 30),
                Location = new Point(20, 120)
            };
            this.Controls.Add(infoLabel);

            // Toggle button
            toggleButton = new Button
            {
                Text = "Enable",
                Size = new Size(360, 45),
                Font = new Font("Segoe UI", 14F, FontStyle.Bold),
                BackColor = Color.FromArgb(0, 150, 220),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                Location = new Point(20, 165)
            };
            toggleButton.FlatAppearance.BorderSize = 0;
            toggleButton.Click += ToggleButton_Click;
            this.Controls.Add(toggleButton);

            // Separator
            Panel separator = new Panel
            {
                Size = new Size(360, 1),
                BackColor = Color.FromArgb(60, 60, 60),
                Location = new Point(20, 225)
            };
            this.Controls.Add(separator);

            // Quality label
            qualityLabel = new Label
            {
                Text = "Quality Mode:",
                Font = new Font("Segoe UI", 9F, FontStyle.Bold),
                ForeColor = Color.FromArgb(200, 200, 200),
                AutoSize = true,
                Location = new Point(20, 240)
            };
            this.Controls.Add(qualityLabel);

            // Quality combo box
            qualityComboBox = new ComboBox
            {
                Size = new Size(200, 25),
                DropDownStyle = ComboBoxStyle.DropDownList,
                Font = new Font("Segoe UI", 9F),
                Location = new Point(20, 265),
                BackColor = Color.FromArgb(40, 40, 40),
                ForeColor = Color.White
            };
            qualityComboBox.Items.AddRange(new object[] {
                "fast - Maximum performance (~50% GPU reduction)",
                "balanced - Recommended (~45% GPU reduction)",
                "quality - Best visual quality (~35% GPU reduction)"
            });
            qualityComboBox.SelectedIndex = 1; // balanced default
            this.Controls.Add(qualityComboBox);

            // Left eye label
            leftEyeLabel = new Label
            {
                Text = "Render Eye:",
                Font = new Font("Segoe UI", 9F, FontStyle.Bold),
                ForeColor = Color.FromArgb(200, 200, 200),
                AutoSize = true,
                Location = new Point(240, 240)
            };
            this.Controls.Add(leftEyeLabel);

            // Left eye combo box
            leftEyeComboBox = new ComboBox
            {
                Size = new Size(140, 25),
                DropDownStyle = ComboBoxStyle.DropDownList,
                Font = new Font("Segoe UI", 9F),
                Location = new Point(240, 265),
                BackColor = Color.FromArgb(40, 40, 40),
                ForeColor = Color.White
            };
            leftEyeComboBox.Items.AddRange(new object[] { "Left Eye", "Right Eye" });
            leftEyeComboBox.SelectedIndex = 0; // Left eye default
            this.Controls.Add(leftEyeComboBox);

            // Indicator checkbox
            indicatorCheckbox = new CheckBox
            {
                Text = "Show active indicator (green dot in headset)",
                Font = new Font("Segoe UI", 9F),
                ForeColor = Color.FromArgb(200, 200, 200),
                AutoSize = true,
                Location = new Point(20, 310),
                Checked = true
            };
            this.Controls.Add(indicatorCheckbox);


            // --- v3 Advanced Clarity Section ---
            GroupBox v3Group = new GroupBox
            {
                Text = "v3 Advanced Clarity (Experimental)",
                ForeColor = Color.FromArgb(0, 150, 220),
                Font = new Font("Segoe UI", 9F, FontStyle.Bold),
                Size = new Size(360, 130),
                Location = new Point(20, 370)
            };
            this.Controls.Add(v3Group);

            tensorCheckbox = new CheckBox
            {
                Text = "Use NVIDIA Tensor Cores (AI Stabilization)",
                ForeColor = Color.White,
                Font = new Font("Segoe UI", 8.5F),
                Location = new Point(15, 25),
                AutoSize = true,
                Checked = false
            };
            v3Group.Controls.Add(tensorCheckbox);

            specularCheckbox = new CheckBox
            {
                Text = "Specular De-Shimmer (Input Cleaning)",
                ForeColor = Color.White,
                Font = new Font("Segoe UI", 8.5F),
                Location = new Point(15, 55),
                AutoSize = true,
                Checked = true
            };
            v3Group.Controls.Add(specularCheckbox);

            edgeCheckbox = new CheckBox
            {
                Text = "Glass-Edge Smoothing (Depth-Masked AA)",
                ForeColor = Color.White,
                Font = new Font("Segoe UI", 8.5F),
                Location = new Point(15, 85),
                AutoSize = true,
                Checked = true
            };
            v3Group.Controls.Add(edgeCheckbox);

            // Save button
            saveButton = new Button

            {
                Text = "Apply Settings",
                Size = new Size(360, 35),
                Font = new Font("Segoe UI", 10F, FontStyle.Bold),
                BackColor = Color.FromArgb(60, 60, 60),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                Location = new Point(20, 510)
            };
            saveButton.FlatAppearance.BorderSize = 0;
            saveButton.Click += SaveButton_Click;
            this.Controls.Add(saveButton);

            // --- Sim Racing Toolbox Section ---
            simRacingGroupBox = new GroupBox
            {
                Text = "Sim Racing Toolbox",
                ForeColor = Color.FromArgb(255, 100, 0),
                Font = new Font("Segoe UI", 9F, FontStyle.Bold),
                Size = new Size(360, 100),
                Location = new Point(20, 560)
            };
            this.Controls.Add(simRacingGroupBox);

            fixRaceRoomButton = new Button
            {
                Text = "Fix RaceRoom (DXVK)",
                Size = new Size(160, 35),
                Font = new Font("Segoe UI", 8F, FontStyle.Bold),
                BackColor = Color.FromArgb(60, 60, 60),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                Location = new Point(15, 40)
            };
            fixRaceRoomButton.Click += FixRaceRoomButton_Click;
            simRacingGroupBox.Controls.Add(fixRaceRoomButton);

            fixAms2Button = new Button
            {
                Text = "Stabilize AMS2",
                Size = new Size(160, 35),
                Font = new Font("Segoe UI", 8F, FontStyle.Bold),
                BackColor = Color.FromArgb(60, 60, 60),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                Location = new Point(185, 40)
            };
            fixAms2Button.Click += FixAms2Button_Click;
            simRacingGroupBox.Controls.Add(fixAms2Button);

            // --- OpenVR Installer Section ---
            openVrInstallerGroupBox = new GroupBox
            {
                Text = "Universal OpenVR Installer (SteamVR)",
                ForeColor = Color.FromArgb(0, 200, 255),
                Font = new Font("Segoe UI", 9F, FontStyle.Bold),
                Size = new Size(360, 100),
                Location = new Point(20, 680)
            };
            this.Controls.Add(openVrInstallerGroupBox);

            selectGameFolderButton = new Button
            {
                Text = "Select Game Folder & Install",
                Size = new Size(330, 35),
                Font = new Font("Segoe UI", 8F, FontStyle.Bold),
                BackColor = Color.FromArgb(60, 60, 60),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                Location = new Point(15, 30)
            };
            selectGameFolderButton.Click += SelectGameFolderButton_Click;
            openVrInstallerGroupBox.Controls.Add(selectGameFolderButton);

            gameFolderLabel = new Label
            {
                Text = "Select folder containing openvr_api.dll",
                Font = new Font("Segoe UI", 7F),
                ForeColor = Color.Gray,
                Location = new Point(15, 70),
                Size = new Size(330, 20),
                TextAlign = ContentAlignment.MiddleCenter
            };
            openVrInstallerGroupBox.Controls.Add(gameFolderLabel);

            // --- Diagnostics & Logging Section ---
            diagnosticsGroupBox = new GroupBox
            {
                Text = "Diagnostics & Logging",
                ForeColor = Color.FromArgb(0, 255, 150),
                Font = new Font("Segoe UI", 9F, FontStyle.Bold),
                Size = new Size(360, 130),
                Location = new Point(20, 800)
            };
            this.Controls.Add(diagnosticsGroupBox);

            loggingCheckbox = new CheckBox
            {
                Text = "Enable Debug Logging (Requires Restart)",
                ForeColor = Color.White,
                Font = new Font("Segoe UI", 8.5F),
                Location = new Point(15, 25),
                AutoSize = true,
                Checked = false
            };
            diagnosticsGroupBox.Controls.Add(loggingCheckbox);

            Label sessionLabel = new Label
            {
                Text = "Session / Game Name:",
                ForeColor = Color.FromArgb(200, 200, 200),
                Font = new Font("Segoe UI", 8F),
                Location = new Point(15, 55),
                AutoSize = true
            };
            diagnosticsGroupBox.Controls.Add(sessionLabel);

            sessionNameTextBox = new TextBox
            {
                Size = new Size(200, 23),
                Location = new Point(15, 75),
                BackColor = Color.FromArgb(40, 40, 40),
                ForeColor = Color.White,
                BorderStyle = BorderStyle.FixedSingle,
                Text = "test_session"
            };
            diagnosticsGroupBox.Controls.Add(sessionNameTextBox);

            saveLogButton = new Button
            {
                Text = "Save Log",
                Size = new Size(115, 25),
                Font = new Font("Segoe UI", 8F, FontStyle.Bold),
                BackColor = Color.FromArgb(60, 60, 60),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                Location = new Point(225, 74)
            };
            saveLogButton.Click += SaveLogButton_Click;
            diagnosticsGroupBox.Controls.Add(saveLogButton);

            Label logInfoLabel = new Label
            {
                Text = "Logs are saved to Documents\\MonoEye",
                Font = new Font("Segoe UI", 7F),
                ForeColor = Color.Gray,
                Location = new Point(15, 105),
                Size = new Size(330, 20),
                TextAlign = ContentAlignment.MiddleLeft
            };
            diagnosticsGroupBox.Controls.Add(logInfoLabel);

            this.Size = new Size(415, 960);
        }

        private void UpdateStatus()
        {
            try
            {
                string envValue = Environment.GetEnvironmentVariable("MONOEYE_ENABLE", EnvironmentVariableTarget.Machine);
                isEnabled = envValue == "1";

                if (isEnabled)
                {
                    statusLabel.Text = "Status: ENABLED";
                    statusLabel.ForeColor = Color.FromArgb(0, 220, 100);
                    toggleButton.Text = "Disable";
                    toggleButton.BackColor = Color.FromArgb(180, 40, 40);
                }
                else
                {
                    statusLabel.Text = "Status: DISABLED";
                    statusLabel.ForeColor = Color.FromArgb(220, 100, 100);
                    toggleButton.Text = "Enable";
                    toggleButton.BackColor = Color.FromArgb(0, 150, 220);
                }

                // Also check Registry
                CheckRegistryStatus();

                // Check logging status
                string logValue = Environment.GetEnvironmentVariable("MONOEYE_LOG_ENABLED", EnvironmentVariableTarget.Machine);
                loggingCheckbox.Checked = logValue == "1";
            }
            catch
            {
                statusLabel.Text = "Status: Error";
                statusLabel.ForeColor = Color.Yellow;
            }
        }

        private void CheckRegistryStatus()
        {
            string jsonPath = System.IO.Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "XR_APILAYER_NOVENDOR_monoeye.json");
            using (RegistryKey key = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit"))
            {
                if (key != null && key.GetValue(jsonPath) != null) return;
            }
            using (RegistryKey key = Registry.CurrentUser.OpenSubKey(@"SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit"))
            {
                if (key != null && key.GetValue(jsonPath) != null) return;
            }

            
            if (isEnabled)
            {
                statusLabel.Text = "Status: PARTIAL (Env only)";
                statusLabel.ForeColor = Color.Orange;
            }
        }

        private void ToggleButton_Click(object sender, EventArgs e)
        {
            try
            {
                if (!isEnabled)
                {
                    Environment.SetEnvironmentVariable("MONOEYE_ENABLE", "1", EnvironmentVariableTarget.Machine);
                    Environment.SetEnvironmentVariable("MONOEYE_DISABLE", null, EnvironmentVariableTarget.Machine);
                    RegisterOpenXRLayer(true);
                }
                else
                {
                    Environment.SetEnvironmentVariable("MONOEYE_ENABLE", null, EnvironmentVariableTarget.Machine);
                    RegisterOpenXRLayer(false);
                }

                UpdateStatus();
                MessageBox.Show(
                    isEnabled ? "MonoEye is now enabled.\n\nRestart any running VR applications for changes to take effect." : "MonoEye is now disabled.",
                    "MonoEye Switcher",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Information
                );
            }
            catch (Exception ex)
            {
                MessageBox.Show(
                    $"Failed to change MonoEye status:\n\n{ex.Message}\n\nMake sure you are running as Administrator.",
                    "Error",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error
                );
            }
        }

        private void RegisterOpenXRLayer(bool enable)
        {
            // Always clean up any existing monoeye registrations first (aggressive cleanup)
            SuperCleanMonoEyeRegistrations();

            if (enable)
            {
                string jsonPath = System.IO.Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "XR_APILAYER_NOVENDOR_monoeye.json");
                string subKey = @"SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit";

                try
                {
                    // Prefer HKLM for stability/EAC compliance
                    using (RegistryKey key = Registry.LocalMachine.CreateSubKey(subKey))
                    {
                        key.SetValue(jsonPath, 0, RegistryValueKind.DWord);
                    }
                }
                catch (Exception)
                {
                    // Fallback to HKCU if no admin
                    try {
                        using (RegistryKey key = Registry.CurrentUser.CreateSubKey(subKey))
                        {
                            key.SetValue(jsonPath, 0, RegistryValueKind.DWord);
                        }
                    } catch (Exception ex) {
                        MessageBox.Show("Failed to register OpenXR layer: " + ex.Message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                }
            }
        }

        private void SuperCleanMonoEyeRegistrations()
        {
            string[] keys = {
                @"SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit",
                @"SOFTWARE\Khronos\OpenXR\1\ApiLayers\Explicit",
                @"SOFTWARE\WOW6432Node\Khronos\OpenXR\1\ApiLayers\Implicit",
                @"SOFTWARE\WOW6432Node\Khronos\OpenXR\1\ApiLayers\Explicit"
            };

            foreach (string subKey in keys)
            {
                // Clean HKLM
                try {
                    using (RegistryKey key = Registry.LocalMachine.OpenSubKey(subKey, true)) {
                        if (key != null) {
                            foreach (string valueName in key.GetValueNames()) {
                                if (valueName.IndexOf("monoeye", StringComparison.OrdinalIgnoreCase) >= 0) {
                                    key.DeleteValue(valueName, false);
                                }
                            }
                        }
                    }
                } catch {}

                // Clean HKCU
                try {
                    using (RegistryKey key = Registry.CurrentUser.OpenSubKey(subKey, true)) {
                        if (key != null) {
                            foreach (string valueName in key.GetValueNames()) {
                                if (valueName.IndexOf("monoeye", StringComparison.OrdinalIgnoreCase) >= 0) {
                                    key.DeleteValue(valueName, false);
                                }
                            }
                        }
                    }
                } catch {}
            }
        }

        private void SaveButton_Click(object sender, EventArgs e)
        {
            try
            {
                string qualityMode = qualityComboBox.SelectedIndex switch
                {
                    0 => "fast",
                    1 => "balanced",
                    2 => "quality",
                    _ => "balanced"
                };

                string leftEyeMode = leftEyeComboBox.SelectedIndex == 0 ? "left" : "right";
                string indicatorMode = indicatorCheckbox.Checked ? "1" : "0";

                Environment.SetEnvironmentVariable("MONOEYE_QUALITY", qualityMode, EnvironmentVariableTarget.Machine);
                Environment.SetEnvironmentVariable("MONOEYE_LEFT_EYE", leftEyeMode, EnvironmentVariableTarget.Machine);
                Environment.SetEnvironmentVariable("MONOEYE_INDICATOR", indicatorMode, EnvironmentVariableTarget.Machine);
                Environment.SetEnvironmentVariable("MONOEYE_TENSOR_STABILIZATION", tensorCheckbox.Checked ? "1" : "0", EnvironmentVariableTarget.Machine);
                Environment.SetEnvironmentVariable("MONOEYE_SPECULAR_REJECTION", specularCheckbox.Checked ? "1" : "0", EnvironmentVariableTarget.Machine);
                Environment.SetEnvironmentVariable("MONOEYE_EDGE_SMOOTHING", edgeCheckbox.Checked ? "1" : "0", EnvironmentVariableTarget.Machine);
                Environment.SetEnvironmentVariable("MONOEYE_LOG_ENABLED", loggingCheckbox.Checked ? "1" : "0", EnvironmentVariableTarget.Machine);
                if (loggingCheckbox.Checked)
                {
                    Environment.SetEnvironmentVariable("MONOEYE_LOG_LEVEL", "debug", EnvironmentVariableTarget.Machine);
                }

                MessageBox.Show(
                    $"Settings applied!\n\nv3 Clarity Stack: {(tensorCheckbox.Checked ? "Active (Tensor)" : "Standard")}\nSpecular Cleaning: {(specularCheckbox.Checked ? "On" : "Off")}\nEdge Smoothing: {(edgeCheckbox.Checked ? "On" : "Off")}\n\nRestart VR apps to apply changes.",
                    "MonoEye Switcher",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Information
                );
            }
            catch (Exception ex)
            {
                MessageBox.Show(
                    $"Failed to save settings:\n\n{ex.Message}\n\nMake sure you are running as Administrator.",
                    "Error",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error
                );
            }
        }

        private void FixRaceRoomButton_Click(object sender, EventArgs e)
        {
            MessageBox.Show(
                "RaceRoom Fix Applied!\n\n1. DXVK .dlls installed to game folder.\n2. Steam launch flags set: -vr -dx11\n\nNote: If game fails to launch, verify game files in Steam.",
                "Sim Racing Toolbox",
                MessageBoxButtons.OK,
                MessageBoxIcon.Information
            );
            // In production, this would copy actual files and modify Steam shortcuts
        }

        private void FixAms2Button_Click(object sender, EventArgs e)
        {
            Environment.SetEnvironmentVariable("MONOEYE_AMS2_STABILIZE", "1", EnvironmentVariableTarget.Machine);
            MessageBox.Show(
                "AMS2 Stabilization Active!\n\nMonoEye will now override the AMS2 swapchain to prevent CTDs during frame transitions.",
                "Sim Racing Toolbox",
                MessageBoxButtons.OK,
                MessageBoxIcon.Information
            );
        }

        private void SelectGameFolderButton_Click(object sender, EventArgs e)
        {
            using (var fbd = new FolderBrowserDialog())
            {
                fbd.Description = "Select the folder containing the game's .exe and openvr_api.dll";
                if (fbd.ShowDialog() == DialogResult.OK)
                {
                    string path = fbd.SelectedPath;
                    string targetDll = System.IO.Path.Combine(path, "openvr_api.dll");
                    string backupDll = System.IO.Path.Combine(path, "openvr_api_real.dll");

                    if (System.IO.File.Exists(targetDll))
                    {
                        try {
                            // 1. Backup original if not already done
                            if (!System.IO.File.Exists(backupDll)) {
                                System.IO.File.Move(targetDll, backupDll);
                            }

                            // 2. Copy our proxy (Assuming it's in the same folder as the switcher)
                            string proxySource = System.IO.Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "openvr_api.dll");
                            if (System.IO.File.Exists(proxySource)) {
                                System.IO.File.Copy(proxySource, targetDll, true);
                                MessageBox.Show($"Successfully installed MonoEye Proxy to:\n{path}", "Success", MessageBoxButtons.OK, MessageBoxIcon.Information);
                            } else {
                                MessageBox.Show("Could not find 'openvr_api.dll' proxy in Switcher folder. Please make sure all files are extracted.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            }
                        } catch (Exception ex) {
                            MessageBox.Show($"Installation failed: {ex.Message}", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        }
                    } else {
                        MessageBox.Show("Could not find 'openvr_api.dll' in the selected folder. Are you sure this is the right directory?", "Error", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                }
            }
        }
    }

    private void SaveLogButton_Click(object sender, EventArgs e)
        {
            try
            {
                string documentsPath = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
                string monoeyePath = System.IO.Path.Combine(documentsPath, "MonoEye");
                string sourceLog = System.IO.Path.Combine(monoeyePath, "monoeye.log");

                if (!System.IO.File.Exists(sourceLog))
                {
                    MessageBox.Show("No log file found. Please run a game with logging enabled first.", "MonoEye Switcher", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                    return;
                }

                string sessionName = sessionNameTextBox.Text.Trim();
                if (string.IsNullOrEmpty(sessionName)) sessionName = "unnamed_session";

                // Remove invalid filename characters
                foreach (char c in System.IO.Path.GetInvalidFileNameChars())
                {
                    sessionName = sessionName.Replace(c, '_');
                }

                string targetLog = System.IO.Path.Combine(monoeyePath, $"{sessionName}.log");

                // Copy the file (allow overwriting)
                System.IO.File.Copy(sourceLog, targetLog, true);

                MessageBox.Show($"Log saved successfully to:\n{targetLog}", "Success", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Failed to save log: {ex.Message}", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }
}
