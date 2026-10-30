# Run the downloaded AppImage in a real NixOS VM with the official FHS wrapper.
{ image, probe, video }:
let
  pkgs = import (builtins.fetchTarball
    "https://github.com/NixOS/nixpkgs/archive/1f607aa79b6356ed45d3cd3c8079c196e79efa2c.tar.gz") { };
in
pkgs.testers.runNixOSTest {
  name = "qemby-appimage-nixos";
  nodes.machine = { pkgs, ... }: {
    virtualisation.memorySize = 3072;
    virtualisation.cores = 2;
    programs.appimage.enable = true;
    programs.appimage.binfmt = true;
    hardware.graphics.enable = true;
    fonts.packages = [ pkgs.dejavu_fonts ];
    environment.systemPackages = with pkgs; [
      python3 binutils xorg.xorgserver xdotool weston
    ];
    system.stateVersion = "26.05";
  };
  testScript = ''
    machine.start()
    machine.wait_for_unit("multi-user.target")
    machine.succeed("install -m755 ${image} /tmp/qemby.AppImage")
    machine.succeed("install -m755 ${probe} /tmp/qEmbyPlaybackProbe; install -m644 ${video} /tmp/sample.mp4")
    machine.succeed("python3 ${./.}/check_appimage.py --image /tmp/qemby.AppImage --probe /tmp/qEmbyPlaybackProbe --video /tmp/sample.mp4 --distro nixos --report /tmp/nixos.json", timeout=480)
    machine.copy_from_vm("/tmp/nixos.json")
  '';
}
