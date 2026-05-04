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
        private Label statusLabel;
        private Label qualityLabel;
        private Label leftEyeLabel;
        private Label titleLabel;
        private Label infoLabel;
        private CheckBox applyButton;

        private bool isEnabled = false;

        public MainForm()
        {
            InitializeComponent();
            UpdateStatus();
        }

        private void InitializeComponent()
        {
            this.Text = "MonoEye Switcher";
            this.Size = new Size(420, 380);
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
            leftEyeComboBox.Items.AddRange(new object[] { "Left eye", "Right eye" });
            leftEyeComboBox.SelectedIndex = 0;
            this.Controls.Add(leftEyeComboBox);

            // Apply button (checkbox)
            applyButton = new CheckBox
            {
                Text = " Apply settings to environment",
                Font = new Font("Segoe UI", 9F),
                ForeColor = Color.FromArgb(180, 180, 180),
                AutoSize = true,
                Location = new Point(20, 300)
            };
            this.Controls.Add(applyButton);

            // Save button
            Button saveButton = new Button
            {
                Text = "Apply Settings",
                Size = new Size(160, 30),
                Font = new Font("Segoe UI", 9F, FontStyle.Bold),
                BackColor = Color.FromArgb(60, 60, 60),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                Location = new Point(220, 295)
            };
            saveButton.FlatAppearance.BorderSize = 0;
            saveButton.Click += SaveButton_Click;
            this.Controls.Add(saveButton);
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
            using (RegistryKey key = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Khronos\OpenXR\1\ApiLayers\Explicit"))
            {
                if (key != null && key.GetValue(jsonPath) != null) return;
            }
            using (RegistryKey key = Registry.CurrentUser.OpenSubKey(@"SOFTWARE\Khronos\OpenXR\1\ApiLayers\Explicit"))
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
        }

        private void RegisterOpenXRLayer(bool enable)
        {
            string jsonPath = System.IO.Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "XR_APILAYER_NOVENDOR_monoeye.json");
            string subKey = @"SOFTWARE\Khronos\OpenXR\1\ApiLayers\Explicit";

            try
            {
                if (enable)
                {
                    using (RegistryKey key = Registry.LocalMachine.CreateSubKey(subKey))
                    {
                        key.SetValue(jsonPath, 0, RegistryValueKind.DWord);
                    }
                }
                else
                {
                    using (RegistryKey key = Registry.LocalMachine.OpenSubKey(subKey, true))
                    {
                        if (key != null) key.DeleteValue(jsonPath, false);
                    }
                    using (RegistryKey key = Registry.CurrentUser.OpenSubKey(subKey, true))
                    {
                        if (key != null) key.DeleteValue(jsonPath, false);
                    }
                }
            }
            catch (Exception ex)
            {
                // Fallback to CurrentUser if LocalMachine fails
                if (enable)
                {
                    using (RegistryKey key = Registry.CurrentUser.CreateSubKey(subKey))
                    {
                        key.SetValue(jsonPath, 0, RegistryValueKind.DWord);
                    }
                }
            }
        }

        private void SaveButton_Click(object sender, EventArgs e)
        {
            if (!applyButton.Checked)
            {
                MessageBox.Show(
                    "Check the 'Apply settings' box first, then click this button to save quality and eye settings.",
                    "MonoEye Switcher",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Information
                );
                return;
            }

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

                Environment.SetEnvironmentVariable("MONOEYE_QUALITY", qualityMode, EnvironmentVariableTarget.Machine);
                Environment.SetEnvironmentVariable("MONOEYE_LEFT_EYE", leftEyeMode, EnvironmentVariableTarget.Machine);

                MessageBox.Show(
                    $"Settings applied:\n\nQuality: {qualityMode}\nRender eye: {leftEyeMode}\n\nRestart any running VR applications for changes to take effect.",
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
    }
}
