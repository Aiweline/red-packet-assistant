import Cocoa
import FlutterMacOS
import XCTest
@testable import 红包助手

class RunnerTests: XCTestCase {

  func testCompletedPacketStatusIsAssociatedWithItsCard() {
    let packet = CGRect(x: 0.52, y: 0.20, width: 0.12, height: 0.04)
    let completed = CGRect(x: 0.55, y: 0.27, width: 0.14, height: 0.04)

    XCTAssertTrue(
      RedPacketOCRPolicy.isSamePacketCard(
        packetBox: packet,
        statusBox: completed,
        windowSize: CGSize(width: 1200, height: 800)
      )
    )
    XCTAssertTrue(RedPacketOCRPolicy.isCompletedLabel("已被领完"))
  }

  func testDistantCompletedStatusDoesNotSuppressANewPacket() {
    let packet = CGRect(x: 0.52, y: 0.20, width: 0.12, height: 0.04)
    let completed = CGRect(x: 0.55, y: 0.52, width: 0.14, height: 0.04)

    XCTAssertFalse(
      RedPacketOCRPolicy.isSamePacketCard(
        packetBox: packet,
        statusBox: completed,
        windowSize: CGSize(width: 1200, height: 800)
      )
    )
  }

  func testOutcomePopupIsRecognizedForAutomaticClose() {
    XCTAssertTrue(RedPacketOCRPolicy.isOutcomeLabel("手慢了，红包派完了"))
    XCTAssertTrue(RedPacketOCRPolicy.isOutcomeLabel("查看领取详情〉"))
    XCTAssertFalse(RedPacketOCRPolicy.isOutcomeLabel("微信红包"))
  }

  func testIncomingPacketOnTheLeftIsStillACandidate() {
    let incomingPacket = CGRect(
      x: 0.208,
      y: 0.189,
      width: 0.129,
      height: 0.083
    )

    XCTAssertTrue(RedPacketOCRPolicy.isPacketPosition(incomingPacket))
  }

  func testSidebarTextIsNotAPacketCandidate() {
    let sidebarText = CGRect(x: 0.02, y: 0.4, width: 0.1, height: 0.04)

    XCTAssertFalse(RedPacketOCRPolicy.isPacketPosition(sidebarText))
  }

  func testPacketPopupHeaderProvidesOpenButtonFallback() {
    let header = CGRect(x: 0.35, y: 0.70, width: 0.16, height: 0.04)
    let point = RedPacketOCRPolicy.fallbackOpenPoint(
      label: "帝·鹿鹿发出的红包",
      box: header
    )

    XCTAssertNotNil(point)
    XCTAssertEqual(point?.x ?? 0, 0.43, accuracy: 0.001)
    XCTAssertEqual(point?.y ?? 0, 0.38, accuracy: 0.001)
  }

  func testChatCardCannotTriggerOpenButtonFallback() {
    let card = CGRect(x: 0.35, y: 0.70, width: 0.16, height: 0.04)

    XCTAssertNil(
      RedPacketOCRPolicy.fallbackOpenPoint(label: "微信红包", box: card)
    )
    XCTAssertNil(
      RedPacketOCRPolicy.fallbackOpenPoint(
        label: "帝·鹿鹿发出的红包",
        box: CGRect(x: 0.01, y: 0.70, width: 0.12, height: 0.04)
      )
    )
  }

}
