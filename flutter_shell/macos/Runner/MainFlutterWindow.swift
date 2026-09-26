import Cocoa
import FlutterMacOS

class MainFlutterWindow: NSWindow {
  override func awakeFromNib() {
    let flutterViewController = FlutterViewController()
    self.contentViewController = flutterViewController

    RegisterGeneratedPlugins(registry: flutterViewController)

    super.awakeFromNib()

    // Slightly tighten the Flutter window content corners while keeping the
    // native macOS window frame and traffic-light controls unchanged.
    self.contentView?.wantsLayer = true
    self.contentView?.layer?.cornerRadius = 10
    self.contentView?.layer?.masksToBounds = true

    let screenFrame = NSScreen.main?.visibleFrame ?? self.frame
    let windowWidth = min(1440, screenFrame.width * 0.92)
    let windowHeight = min(900, screenFrame.height * 0.92)
    self.minSize = NSSize(
      width: min(1150, screenFrame.width),
      height: min(700, screenFrame.height)
    )
    self.setFrame(
      NSRect(x: 0, y: 0, width: windowWidth, height: windowHeight),
      display: true
    )
    self.center()
  }
}
