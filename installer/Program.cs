using System;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Windows.Forms;
using Microsoft.Win32;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace MonoEyeInstaller
{
    public class Program
    {
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new InstallerForm());
        }
    }

    public class InstallerForm : Form
    {
        private ProgressBar progressBar;
        private Label statusLabel;
        private Button installButton;
        private Label logLabel;
        private string installDir = @"C:\Program Files\MonoEye";

        public InstallerForm()
        {
            InitializeComponent();
        }

        private void InitializeComponent()
        {
            this.Text = "MonoEye Setup v0.5.48";
            this.Size = new Size(500, 350);
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.StartPosition = FormStartPosition.CenterScreen;
            this.BackColor = Color.FromArgb(25, 25, 25);
            this.ForeColor = Color.White;
            this.Font = new Font("Segoe UI", 9F);

            Label title = new Label
            {
                Text = "Install MonoEye",
                Font = new Font("Segoe UI", 18F, FontStyle.Bold),
                ForeColor = Color.FromArgb(0, 180, 255),
                AutoSize = true,
                Location = new Point(20, 20)
            };
            this.Controls.Add(title);

            Label desc = new Label
            {
                Text = "This will install the MonoEye OpenXR API Layer to your system.\nAdministrator privileges are required.",
                Location = new Point(25, 65),
                Size = new Size(450, 40),
                ForeColor = Color.FromArgb(200, 200, 200)
            };
            this.Controls.Add(desc);

            statusLabel = new Label
            {
                Text = "Ready to install.",
                Location = new Point(25, 120),
                Size = new Size(450, 20),
                ForeColor = Color.FromArgb(0, 255, 150)
            };
            this.Controls.Add(statusLabel);

            progressBar = new ProgressBar
            {
                Location = new Point(25, 150),
                Size = new Size(435, 25),
                Style = ProgressBarStyle.Continuous
            };
            this.Controls.Add(progressBar);

            logLabel = new Label
            {
                Text = "",
                Location = new Point(25, 185),
                Size = new Size(450, 60),
                Font = new Font("Consolas", 8F),
                ForeColor = Color.Gray
            };
            this.Controls.Add(logLabel);

            installButton = new Button
            {
                Text = "Install Now",
                Location = new Point(360, 260),
                Size = new Size(100, 35),
                FlatStyle = FlatStyle.Flat,
                BackColor = Color.FromArgb(0, 120, 215),
                Cursor = Cursors.Hand
            };
            installButton.FlatAppearance.BorderSize = 0;
            installButton.Click += async (s, e) => await StartInstallation();
            this.Controls.Add(installButton);

            Button cancelButton = new Button
            {
                Text = "Cancel",
                Location = new Point(250, 260),
                Size = new Size(100, 35),
                FlatStyle = FlatStyle.Flat,
                ForeColor = Color.Gray
            };
            cancelButton.FlatAppearance.BorderColor = Color.Gray;
            cancelButton.Click += (s, e) => this.Close();
            this.Controls.Add(cancelButton);
        }

        private void Log(string msg)
        {
            if (this.InvokeRequired)
            {
                this.Invoke(new Action<string>(Log), msg);
                return;
            }
            logLabel.Text = msg + "\n" + logLabel.Text;
        }

        private async Task StartInstallation()
        {
            installButton.Enabled = false;
            progressBar.Value = 0;

            try
            {
                // 1. Create Directory
                statusLabel.Text = "Creating directories...";
                Log($"Target: {installDir}");
                if (!Directory.Exists(installDir)) Directory.CreateDirectory(installDir);
                progressBar.Value = 20;
                await Task.Delay(500);

                // 2. Extract Files
                statusLabel.Text = "Extracting binaries...";
                ExtractResource("XR_APILAYER_NOVENDOR_monoeye.dll", Path.Combine(installDir, "XR_APILAYER_NOVENDOR_monoeye.dll"));
                ExtractResource("MonoEyeSwitcher.exe", Path.Combine(installDir, "MonoEyeSwitcher.exe"));
                progressBar.Value = 50;
                await Task.Delay(500);

                // 3. Update Manifest with Absolute Path
                statusLabel.Text = "Configuring OpenXR manifest...";
                string jsonPath = Path.Combine(installDir, "XR_APILAYER_NOVENDOR_monoeye.json");
                string jsonContent = GetResourceString("XR_APILAYER_NOVENDOR_monoeye.json");
                
                // Ensure absolute path is used
                string escapedPath = installDir.Replace("\\", "\\\\");
                jsonContent = jsonContent.Replace(".\\\\XR_APILAYER_NOVENDOR_monoeye.dll", escapedPath + "\\\\XR_APILAYER_NOVENDOR_monoeye.dll");
                File.WriteAllText(jsonPath, jsonContent);
                Log("Manifest updated with absolute path.");
                progressBar.Value = 70;
                await Task.Delay(500);

                // 4. Registry Registration
                statusLabel.Text = "Registering API Layer...";
                using (var key = Registry.LocalMachine.CreateSubKey(@"SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit"))
                {
                    key.SetValue(jsonPath, 0, RegistryValueKind.DWord);
                }
                Log("Registry keys created in HKLM.");
                progressBar.Value = 85;
                await Task.Delay(500);

                // 5. Shortcuts and PATH
                statusLabel.Text = "Finalizing...";
                CreateShortcut("MonoEye Switcher", Path.Combine(installDir, "MonoEyeSwitcher.exe"));
                AddToPath(installDir);
                progressBar.Value = 100;

                statusLabel.Text = "Installation Complete!";
                statusLabel.ForeColor = Color.Lime;
                Log("MonoEye is now active.");
                
                MessageBox.Show("MonoEye has been successfully installed.\n\nYou can now enable/disable it using the Switcher on your desktop.", "Success", MessageBoxButtons.OK, MessageBoxIcon.Information);
                this.Close();
            }
            catch (Exception ex)
            {
                statusLabel.Text = "Installation Failed!";
                statusLabel.ForeColor = Color.Red;
                Log($"ERROR: {ex.Message}");
                MessageBox.Show($"Installation failed: {ex.Message}", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                installButton.Enabled = true;
            }
        }

        private void ExtractResource(string name, string dest)
        {
            var assembly = Assembly.GetExecutingAssembly();
            string resourceName = null;
            foreach (var r in assembly.GetManifestResourceNames())
            {
                if (r.EndsWith(name)) { resourceName = r; break; }
            }

            if (resourceName == null)
            {
                Log($"Warning: Resource {name} not found in installer.");
                return;
            }

            using (Stream stream = assembly.GetManifestResourceStream(resourceName))
            using (FileStream fs = new FileStream(dest, FileMode.Create))
            {
                stream.CopyTo(fs);
            }
            Log($"Extracted: {name}");
        }

        private string GetResourceString(string name)
        {
            var assembly = Assembly.GetExecutingAssembly();
            string resourceName = null;
            foreach (var r in assembly.GetManifestResourceNames())
            {
                if (r.EndsWith(name)) { resourceName = r; break; }
            }
            if (resourceName == null) return "";

            using (Stream stream = assembly.GetManifestResourceStream(resourceName))
            using (StreamReader reader = new StreamReader(stream))
            {
                return reader.ReadToEnd();
            }
        }

        private void CreateShortcut(string name, string targetPath)
        {
            try
            {
                string desktop = Environment.GetFolderPath(Environment.SpecialFolder.PublicDesktop);
                if (!Directory.Exists(desktop)) desktop = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);
                
                string shortcutPath = Path.Combine(desktop, name + ".lnk");
                
                // Use VBScript fallback to avoid external dependencies
                string script = $@"
                    Set sh = CreateObject(""WScript.Shell"")
                    Set shortcut = sh.CreateShortcut(""{shortcutPath}"")
                    shortcut.TargetPath = ""{targetPath}""
                    shortcut.WorkingDirectory = ""{Path.GetDirectoryName(targetPath)}""
                    shortcut.Save";
                
                string scriptPath = Path.GetTempFileName() + ".vbs";
                File.WriteAllText(scriptPath, script);
                
                var process = System.Diagnostics.Process.Start("cscript.exe", $"//nologo \"{scriptPath}\"");
                process.WaitForExit();
                File.Delete(scriptPath);
                
                Log("Shortcut created on desktop.");
            }
            catch { Log("Failed to create shortcut."); }
        }

        private void AddToPath(string path)
        {
            try
            {
                var name = "PATH";
                var scope = EnvironmentVariableTarget.Machine;
                var oldValue = Environment.GetEnvironmentVariable(name, scope);
                if (!oldValue.Contains(path))
                {
                    var newValue = oldValue + ";" + path;
                    Environment.SetEnvironmentVariable(name, newValue, scope);
                    Log("Added to system PATH.");
                }
            }
            catch { Log("Failed to update PATH."); }
        }
    }
}
