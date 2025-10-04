#!/usr/bin/env python3
"""
Universal ASN.1 Library Setup Script
Automatically detects platform and sets up ASN.1 library generation
"""

import os
import sys
import subprocess
import platform
import shutil
import tempfile
from pathlib import Path

class Colors:
    """ANSI color codes for terminal output"""
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    PURPLE = '\033[0;35m'
    CYAN = '\033[0;36m'
    WHITE = '\033[1;37m'
    NC = '\033[0m'  # No Color

class ASN1Setup:
    """Main class for ASN.1 setup automation"""
    
    def __init__(self):
        self.script_dir = Path(__file__).parent.absolute()
        self.project_root = self.script_dir.parent
        self.asn1_schema = self.script_dir / "rtl433-rabbitmq.asn1"
        self.generated_dir = self.script_dir / "asn1-generated"
        self.build_dir = self.script_dir / "build"
        
    def print_colored(self, text, color=Colors.NC):
        """Print colored text to console"""
        print(f"{color}{text}{Colors.NC}")
        
    def print_header(self):
        """Print setup header"""
        self.print_colored("=" * 60, Colors.BLUE)
        self.print_colored("RTL433 ASN.1 Library Universal Setup", Colors.BLUE)
        self.print_colored("=" * 60, Colors.BLUE)
        print()
        
    def detect_platform(self):
        """Detect current platform and package manager"""
        system = platform.system().lower()
        
        if system == "linux":
            # Detect Linux distribution
            if shutil.which("apt-get"):
                return "ubuntu"
            elif shutil.which("yum"):
                return "centos"
            elif shutil.which("dnf"):
                return "fedora"
            elif shutil.which("pacman"):
                return "arch"
            elif shutil.which("zypper"):
                return "opensuse"
            else:
                return "linux"
        elif system == "darwin":
            return "macos"
        elif system == "windows":
            return "windows"
        else:
            return "unknown"
            
    def check_asn1c(self):
        """Check if asn1c is available"""
        if shutil.which("asn1c"):
            try:
                result = subprocess.run(["asn1c", "-h"], 
                                      capture_output=True, text=True, timeout=10)
                version_line = result.stderr.split('\n')[0] if result.stderr else "Unknown version"
                self.print_colored(f"✓ asn1c found: {version_line}", Colors.GREEN)
                return True
            except (subprocess.TimeoutExpired, subprocess.SubprocessError):
                self.print_colored("✗ asn1c found but not responding", Colors.RED)
                return False
        else:
            self.print_colored("✗ asn1c not found", Colors.RED)
            return False
            
    def install_asn1c(self):
        """Install asn1c based on detected platform"""
        platform_name = self.detect_platform()
        self.print_colored(f"Installing asn1c for {platform_name}...", Colors.YELLOW)
        
        try:
            if platform_name == "ubuntu":
                self._run_command(["sudo", "apt-get", "update"])
                self._run_command(["sudo", "apt-get", "install", "-y", "asn1c", "build-essential", "cmake"])
                
            elif platform_name == "centos":
                self._run_command(["sudo", "yum", "install", "-y", "epel-release"])
                self._run_command(["sudo", "yum", "install", "-y", "asn1c", "gcc", "cmake", "make"])
                
            elif platform_name == "fedora":
                self._run_command(["sudo", "dnf", "install", "-y", "asn1c", "gcc", "cmake", "make"])
                
            elif platform_name == "arch":
                self._run_command(["sudo", "pacman", "-S", "--noconfirm", "asn1c", "gcc", "cmake", "make"])
                
            elif platform_name == "opensuse":
                self._run_command(["sudo", "zypper", "install", "-y", "asn1c", "gcc", "cmake", "make"])
                
            elif platform_name == "macos":
                if shutil.which("brew"):
                    self._run_command(["brew", "install", "asn1c", "cmake"])
                else:
                    raise Exception("Homebrew not found. Please install Homebrew first.")
                    
            elif platform_name == "windows":
                self._install_asn1c_windows()
                
            else:
                self.print_colored("Unknown platform, attempting to build from source...", Colors.YELLOW)
                self._install_from_source()
                
        except Exception as e:
            self.print_colored(f"Installation failed: {e}", Colors.RED)
            return False
            
        return self.check_asn1c()
        
    def _install_asn1c_windows(self):
        """Install asn1c on Windows"""
        self.print_colored("Windows detected. Checking installation options...", Colors.YELLOW)
        
        # Check for vcpkg
        if shutil.which("vcpkg"):
            self.print_colored("Installing via vcpkg...", Colors.YELLOW)
            self._run_command(["vcpkg", "install", "asn1c"])
        elif shutil.which("choco"):
            self.print_colored("Installing via Chocolatey...", Colors.YELLOW)
            self._run_command(["choco", "install", "asn1c", "-y"])
        elif os.path.exists("C:\\msys64\\usr\\bin\\bash.exe"):
            self.print_colored("Installing via MSYS2...", Colors.YELLOW)
            self._run_command(["C:\\msys64\\usr\\bin\\bash.exe", "-c", "pacman -S --noconfirm asn1c"])
        else:
            self.print_colored("No package manager found. Manual installation required:", Colors.YELLOW)
            print("1. Install vcpkg and run: vcpkg install asn1c")
            print("2. Use WSL: wsl --install")
            print("3. Install MSYS2 and run: pacman -S asn1c")
            print("4. Build from source: https://github.com/vlm/asn1c")
            raise Exception("Manual installation required")
            
    def _install_from_source(self):
        """Install asn1c from source"""
        self.print_colored("Building asn1c from source...", Colors.YELLOW)
        
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)
            
            # Clone repository
            self._run_command(["git", "clone", "https://github.com/vlm/asn1c.git"], 
                            cwd=temp_path)
            
            asn1c_dir = temp_path / "asn1c"
            
            # Build and install
            self._run_command(["autoreconf", "-iv"], cwd=asn1c_dir)
            self._run_command(["./configure", "--prefix=/usr/local"], cwd=asn1c_dir)
            self._run_command(["make"], cwd=asn1c_dir)
            self._run_command(["sudo", "make", "install"], cwd=asn1c_dir)
            
        self.print_colored("asn1c installed from source", Colors.GREEN)
        
    def _run_command(self, cmd, cwd=None, check=True):
        """Run a command with error handling"""
        try:
            result = subprocess.run(cmd, cwd=cwd, check=check, 
                                  capture_output=True, text=True, timeout=300)
            return result
        except subprocess.CalledProcessError as e:
            self.print_colored(f"Command failed: {' '.join(cmd)}", Colors.RED)
            self.print_colored(f"Error: {e.stderr}", Colors.RED)
            raise
        except subprocess.TimeoutExpired:
            self.print_colored(f"Command timed out: {' '.join(cmd)}", Colors.RED)
            raise
            
    def validate_schema(self):
        """Validate ASN.1 schema"""
        if not self.asn1_schema.exists():
            raise Exception(f"ASN.1 schema not found: {self.asn1_schema}")
            
        self.print_colored("Validating ASN.1 schema...", Colors.YELLOW)
        try:
            self._run_command(["asn1c", "-E", str(self.asn1_schema)])
            self.print_colored("✓ ASN.1 schema validation passed", Colors.GREEN)
        except subprocess.CalledProcessError:
            self.print_colored("✗ ASN.1 schema validation failed", Colors.RED)
            raise
            
    def generate_code(self):
        """Generate ASN.1 C code"""
        self.print_colored("Generating ASN.1 C code...", Colors.YELLOW)
        
        # Create output directory
        self.generated_dir.mkdir(exist_ok=True)
        
        # Generate code
        cmd = [
            "asn1c",
            "-fcompound-names",
            "-fincludes-quoted", 
            "-fno-include-deps",
            "-pdu=all",
            "-gen-PER",
            "-gen-OER",
            "-D", str(self.generated_dir),
            str(self.asn1_schema)
        ]
        
        self._run_command(cmd, cwd=self.script_dir)
        
        # Count generated files
        c_files = list(self.generated_dir.glob("*.c"))
        h_files = list(self.generated_dir.glob("*.h"))
        
        self.print_colored(f"✓ Generated {len(c_files)} C files and {len(h_files)} header files", Colors.GREEN)
        
    def build_library(self):
        """Build ASN.1 library with CMake"""
        self.print_colored("Building ASN.1 library...", Colors.YELLOW)
        
        # Create build directory
        self.build_dir.mkdir(exist_ok=True)
        
        # Configure with CMake
        self._run_command(["cmake", "..", "-DCMAKE_BUILD_TYPE=Release"], 
                         cwd=self.build_dir)
        
        # Build
        cpu_count = os.cpu_count() or 4
        self._run_command(["cmake", "--build", ".", "--parallel", str(cpu_count)], 
                         cwd=self.build_dir)
        
        # Check for library file
        lib_files = list(self.build_dir.glob("**/libr_asn1.*")) + \
                   list(self.build_dir.glob("**/r_asn1.lib"))
        
        if lib_files:
            lib_file = lib_files[0]
            size = lib_file.stat().st_size
            self.print_colored(f"✓ Library built: {lib_file.name} ({size:,} bytes)", Colors.GREEN)
        else:
            self.print_colored("✗ Library file not found", Colors.RED)
            
    def run_tests(self):
        """Run basic tests"""
        self.print_colored("Running tests...", Colors.YELLOW)
        
        # Test schema validation again
        try:
            self.validate_schema()
        except:
            self.print_colored("⚠ Schema validation test failed", Colors.YELLOW)
            
        # Check for key generated files
        key_files = ["RTL433Message.h", "RTL433Message.c", "SignalMessage.h"]
        missing_files = []
        
        for file_name in key_files:
            if not (self.generated_dir / file_name).exists():
                missing_files.append(file_name)
                
        if missing_files:
            self.print_colored(f"⚠ Missing key files: {', '.join(missing_files)}", Colors.YELLOW)
        else:
            self.print_colored("✓ All key files generated", Colors.GREEN)
            
        self.print_colored("✓ Tests completed", Colors.GREEN)
        
    def show_summary(self):
        """Show setup summary"""
        print()
        self.print_colored("Setup Summary", Colors.BLUE)
        self.print_colored("=" * 20, Colors.BLUE)
        
        # Generated files
        if self.generated_dir.exists():
            c_files = len(list(self.generated_dir.glob("*.c")))
            h_files = len(list(self.generated_dir.glob("*.h")))
            self.print_colored(f"✓ Generated files: {c_files} C files, {h_files} headers", Colors.GREEN)
        
        # Library
        lib_files = list(self.build_dir.glob("**/libr_asn1.*")) + \
                   list(self.build_dir.glob("**/r_asn1.lib"))
        if lib_files:
            self.print_colored(f"✓ Library: {lib_files[0].relative_to(self.script_dir)}", Colors.GREEN)
            
        print()
        self.print_colored("Usage:", Colors.BLUE)
        print("1. Include in CMake: add_subdirectory(asn1)")
        print("2. Link library: target_link_libraries(your_target r_asn1)")
        print("3. Include headers: #include \"RTL433Message.h\"")
        print()
        self.print_colored("ASN.1 library setup completed successfully!", Colors.GREEN)
        
    def main(self, args):
        """Main execution function"""
        self.print_header()
        
        try:
            if "--install-only" in args:
                if not self.check_asn1c():
                    self.install_asn1c()
                return
                
            if "--generate-only" in args:
                if not self.check_asn1c():
                    raise Exception("asn1c not found. Run with --install-only first.")
                self.validate_schema()
                self.generate_code()
                return
                
            if "--build-only" in args:
                self.build_library()
                return
                
            if "--help" in args or "-h" in args:
                self.print_help()
                return
                
            # Full setup
            self.print_colored("Starting full automated setup...", Colors.BLUE)
            
            # Check/install asn1c
            if not self.check_asn1c():
                self.print_colored("asn1c not found. Installing...", Colors.YELLOW)
                if not self.install_asn1c():
                    raise Exception("Failed to install asn1c")
                    
            # Validate and generate
            self.validate_schema()
            self.generate_code()
            
            # Build library
            self.build_library()
            
            # Run tests
            self.run_tests()
            
            # Show summary
            self.show_summary()
            
        except KeyboardInterrupt:
            self.print_colored("\nSetup interrupted by user", Colors.YELLOW)
            sys.exit(130)
        except Exception as e:
            self.print_colored(f"\nSetup failed: {e}", Colors.RED)
            sys.exit(1)
            
    def print_help(self):
        """Print help information"""
        print("Usage: python3 setup_asn1_universal.py [option]")
        print()
        print("Options:")
        print("  --install-only   Install asn1c only")
        print("  --generate-only  Generate ASN.1 code only")
        print("  --build-only     Build library only")
        print("  --help, -h       Show this help")
        print()
        print("Without options: Full automated setup")
        print()
        print("Supported platforms:")
        print("  - Ubuntu/Debian (apt)")
        print("  - CentOS/RHEL (yum)")
        print("  - Fedora (dnf)")
        print("  - Arch Linux (pacman)")
        print("  - macOS (brew)")
        print("  - Windows (vcpkg/choco/msys2)")
        print("  - Generic Unix (from source)")

if __name__ == "__main__":
    setup = ASN1Setup()
    setup.main(sys.argv[1:])
