import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_localizations/flutter_localizations.dart';
import 'package:intl/intl.dart' as intl;

import 'app_localizations_en.dart';
import 'app_localizations_zh.dart';

// ignore_for_file: type=lint

/// Callers can lookup localized strings with an instance of MgLocalizations
/// returned by `MgLocalizations.of(context)`.
///
/// Applications need to include `MgLocalizations.delegate()` in their app's
/// `localizationDelegates` list, and the locales they support in the app's
/// `supportedLocales` list. For example:
///
/// ```dart
/// import 'generated/app_localizations.dart';
///
/// return MaterialApp(
///   localizationsDelegates: MgLocalizations.localizationsDelegates,
///   supportedLocales: MgLocalizations.supportedLocales,
///   home: MyApplicationHome(),
/// );
/// ```
///
/// ## Update pubspec.yaml
///
/// Please make sure to update your pubspec.yaml to include the following
/// packages:
///
/// ```yaml
/// dependencies:
///   # Internationalization support.
///   flutter_localizations:
///     sdk: flutter
///   intl: any # Use the pinned version from flutter_localizations
///
///   # Rest of dependencies
/// ```
///
/// ## iOS Applications
///
/// iOS applications define key application metadata, including supported
/// locales, in an Info.plist file that is built into the application bundle.
/// To configure the locales supported by your app, you’ll need to edit this
/// file.
///
/// First, open your project’s ios/Runner.xcworkspace Xcode workspace file.
/// Then, in the Project Navigator, open the Info.plist file under the Runner
/// project’s Runner folder.
///
/// Next, select the Information Property List item, select Add Item from the
/// Editor menu, then select Localizations from the pop-up menu.
///
/// Select and expand the newly-created Localizations item then, for each
/// locale your application supports, add a new item and select the locale
/// you wish to add from the pop-up menu in the Value field. This list should
/// be consistent with the languages listed in the MgLocalizations.supportedLocales
/// property.
abstract class MgLocalizations {
  MgLocalizations(String locale)
    : localeName = intl.Intl.canonicalizedLocale(locale.toString());

  final String localeName;

  static MgLocalizations of(BuildContext context) {
    return Localizations.of<MgLocalizations>(context, MgLocalizations)!;
  }

  static const LocalizationsDelegate<MgLocalizations> delegate =
      _MgLocalizationsDelegate();

  /// A list of this localizations delegate along with the default localizations
  /// delegates.
  ///
  /// Returns a list of localizations delegates containing this delegate along with
  /// GlobalMaterialLocalizations.delegate, GlobalCupertinoLocalizations.delegate,
  /// and GlobalWidgetsLocalizations.delegate.
  ///
  /// Additional delegates can be added by appending to this list in
  /// MaterialApp. This list does not have to be used at all if a custom list
  /// of delegates is preferred or required.
  static const List<LocalizationsDelegate<dynamic>> localizationsDelegates =
      <LocalizationsDelegate<dynamic>>[
        delegate,
        GlobalMaterialLocalizations.delegate,
        GlobalCupertinoLocalizations.delegate,
        GlobalWidgetsLocalizations.delegate,
      ];

  /// A list of this localizations delegate's supported locales.
  static const List<Locale> supportedLocales = <Locale>[
    Locale('zh'),
    Locale('en'),
  ];

  /// No description provided for @appTitle.
  ///
  /// In zh, this message translates to:
  /// **'M-GCS'**
  String get appTitle;

  /// No description provided for @navConnection.
  ///
  /// In zh, this message translates to:
  /// **'连接'**
  String get navConnection;

  /// No description provided for @navMission.
  ///
  /// In zh, this message translates to:
  /// **'任务'**
  String get navMission;

  /// No description provided for @navDashboard.
  ///
  /// In zh, this message translates to:
  /// **'仪表'**
  String get navDashboard;

  /// No description provided for @navBackstage.
  ///
  /// In zh, this message translates to:
  /// **'后台'**
  String get navBackstage;

  /// No description provided for @returnFrontstage.
  ///
  /// In zh, this message translates to:
  /// **'返回前台'**
  String get returnFrontstage;

  /// No description provided for @statusGps.
  ///
  /// In zh, this message translates to:
  /// **'GPS 信息'**
  String get statusGps;

  /// No description provided for @statusBattery.
  ///
  /// In zh, this message translates to:
  /// **'电池信息'**
  String get statusBattery;

  /// No description provided for @statusFlightMode.
  ///
  /// In zh, this message translates to:
  /// **'飞行模式'**
  String get statusFlightMode;

  /// No description provided for @statusArmed.
  ///
  /// In zh, this message translates to:
  /// **'解锁状态'**
  String get statusArmed;

  /// No description provided for @statusFlightController.
  ///
  /// In zh, this message translates to:
  /// **'飞控信息'**
  String get statusFlightController;

  /// No description provided for @noData.
  ///
  /// In zh, this message translates to:
  /// **'无数据'**
  String get noData;

  /// No description provided for @currentMode.
  ///
  /// In zh, this message translates to:
  /// **'当前模式'**
  String get currentMode;

  /// No description provided for @armed.
  ///
  /// In zh, this message translates to:
  /// **'已解锁'**
  String get armed;

  /// No description provided for @disarmed.
  ///
  /// In zh, this message translates to:
  /// **'未解锁'**
  String get disarmed;

  /// No description provided for @vehicleArmed.
  ///
  /// In zh, this message translates to:
  /// **'飞行器已解锁'**
  String get vehicleArmed;

  /// No description provided for @safe.
  ///
  /// In zh, this message translates to:
  /// **'安全'**
  String get safe;

  /// No description provided for @messagesCount.
  ///
  /// In zh, this message translates to:
  /// **'{count}条消息'**
  String messagesCount(int count);

  /// No description provided for @notConnected.
  ///
  /// In zh, this message translates to:
  /// **'未连接'**
  String get notConnected;

  /// No description provided for @clickToView.
  ///
  /// In zh, this message translates to:
  /// **'点击查看'**
  String get clickToView;

  /// No description provided for @vehicleNotConnected.
  ///
  /// In zh, this message translates to:
  /// **'飞控未连接'**
  String get vehicleNotConnected;

  /// No description provided for @notSelected.
  ///
  /// In zh, this message translates to:
  /// **'未选择'**
  String get notSelected;

  /// No description provided for @unidentifiedVehicle.
  ///
  /// In zh, this message translates to:
  /// **'未识别飞控'**
  String get unidentifiedVehicle;

  /// No description provided for @noMission.
  ///
  /// In zh, this message translates to:
  /// **'当前无任务'**
  String get noMission;

  /// No description provided for @missionEmptyMessage.
  ///
  /// In zh, this message translates to:
  /// **'新建任务、导入任务，或从飞控下载现有任务。'**
  String get missionEmptyMessage;

  /// No description provided for @disconnected.
  ///
  /// In zh, this message translates to:
  /// **'飞控连接已断开'**
  String get disconnected;

  /// No description provided for @waitingForConnection.
  ///
  /// In zh, this message translates to:
  /// **'等待连接'**
  String get waitingForConnection;

  /// No description provided for @chooseConnection.
  ///
  /// In zh, this message translates to:
  /// **'请选择连接方式'**
  String get chooseConnection;

  /// No description provided for @connectedFlightController.
  ///
  /// In zh, this message translates to:
  /// **'已连接飞控'**
  String get connectedFlightController;

  /// No description provided for @airframePending.
  ///
  /// In zh, this message translates to:
  /// **'机架待确认'**
  String get airframePending;

  /// No description provided for @firmwarePending.
  ///
  /// In zh, this message translates to:
  /// **'固件待确认'**
  String get firmwarePending;

  /// No description provided for @versionPending.
  ///
  /// In zh, this message translates to:
  /// **'版本待确认'**
  String get versionPending;

  /// No description provided for @linkMeasuring.
  ///
  /// In zh, this message translates to:
  /// **'链路 测量中'**
  String get linkMeasuring;

  /// No description provided for @linkPercent.
  ///
  /// In zh, this message translates to:
  /// **'链路 {percent}%'**
  String linkPercent(int percent);

  /// No description provided for @remoteUnavailable.
  ///
  /// In zh, this message translates to:
  /// **'遥控器 不可用'**
  String get remoteUnavailable;

  /// No description provided for @remotePercent.
  ///
  /// In zh, this message translates to:
  /// **'遥控器 {percent}%'**
  String remotePercent(int percent);

  /// No description provided for @remoteRssi.
  ///
  /// In zh, this message translates to:
  /// **'遥控器 RSSI {rssi}'**
  String remoteRssi(int rssi);

  /// No description provided for @requestingParameters.
  ///
  /// In zh, this message translates to:
  /// **'正在请求参数列表'**
  String get requestingParameters;

  /// No description provided for @parametersProgress.
  ///
  /// In zh, this message translates to:
  /// **'{received} / {total}'**
  String parametersProgress(int received, int total);

  /// No description provided for @gpsNotLocated.
  ///
  /// In zh, this message translates to:
  /// **'未定位'**
  String get gpsNotLocated;

  /// No description provided for @satelliteCount.
  ///
  /// In zh, this message translates to:
  /// **'{count}颗'**
  String satelliteCount(int count);

  /// No description provided for @connect.
  ///
  /// In zh, this message translates to:
  /// **'连接'**
  String get connect;

  /// No description provided for @disconnect.
  ///
  /// In zh, this message translates to:
  /// **'断开'**
  String get disconnect;

  /// No description provided for @cancelConnection.
  ///
  /// In zh, this message translates to:
  /// **'取消连接'**
  String get cancelConnection;

  /// No description provided for @serial.
  ///
  /// In zh, this message translates to:
  /// **'Serial'**
  String get serial;

  /// No description provided for @udp.
  ///
  /// In zh, this message translates to:
  /// **'UDP'**
  String get udp;

  /// No description provided for @tcp.
  ///
  /// In zh, this message translates to:
  /// **'TCP'**
  String get tcp;

  /// No description provided for @serialPort.
  ///
  /// In zh, this message translates to:
  /// **'串口连接'**
  String get serialPort;

  /// No description provided for @port.
  ///
  /// In zh, this message translates to:
  /// **'端口'**
  String get port;

  /// No description provided for @baudRate.
  ///
  /// In zh, this message translates to:
  /// **'波特率'**
  String get baudRate;

  /// No description provided for @listenMode.
  ///
  /// In zh, this message translates to:
  /// **'监听模式'**
  String get listenMode;

  /// No description provided for @remoteAddress.
  ///
  /// In zh, this message translates to:
  /// **'远端地址'**
  String get remoteAddress;

  /// No description provided for @close.
  ///
  /// In zh, this message translates to:
  /// **'关闭'**
  String get close;

  /// No description provided for @closeConnectionPanel.
  ///
  /// In zh, this message translates to:
  /// **'关闭连接面板'**
  String get closeConnectionPanel;

  /// No description provided for @selectSerialPort.
  ///
  /// In zh, this message translates to:
  /// **'请选择串口'**
  String get selectSerialPort;

  /// No description provided for @udpListen.
  ///
  /// In zh, this message translates to:
  /// **'UDP 监听'**
  String get udpListen;

  /// No description provided for @tcpConnection.
  ///
  /// In zh, this message translates to:
  /// **'TCP 连接'**
  String get tcpConnection;

  /// No description provided for @mission.
  ///
  /// In zh, this message translates to:
  /// **'航点任务'**
  String get mission;

  /// No description provided for @addWaypoint.
  ///
  /// In zh, this message translates to:
  /// **'添加航点'**
  String get addWaypoint;

  /// No description provided for @editWaypoint.
  ///
  /// In zh, this message translates to:
  /// **'编辑航点'**
  String get editWaypoint;

  /// No description provided for @read.
  ///
  /// In zh, this message translates to:
  /// **'读取'**
  String get read;

  /// No description provided for @write.
  ///
  /// In zh, this message translates to:
  /// **'写入'**
  String get write;

  /// No description provided for @clear.
  ///
  /// In zh, this message translates to:
  /// **'清空'**
  String get clear;

  /// No description provided for @startMission.
  ///
  /// In zh, this message translates to:
  /// **'开始任务'**
  String get startMission;

  /// No description provided for @newMission.
  ///
  /// In zh, this message translates to:
  /// **'新建任务'**
  String get newMission;

  /// No description provided for @importMission.
  ///
  /// In zh, this message translates to:
  /// **'导入任务'**
  String get importMission;

  /// No description provided for @downloadFromVehicle.
  ///
  /// In zh, this message translates to:
  /// **'从飞控下载'**
  String get downloadFromVehicle;

  /// No description provided for @backstage.
  ///
  /// In zh, this message translates to:
  /// **'后台'**
  String get backstage;

  /// No description provided for @parameters.
  ///
  /// In zh, this message translates to:
  /// **'参数配置'**
  String get parameters;

  /// No description provided for @airframe.
  ///
  /// In zh, this message translates to:
  /// **'机架类型'**
  String get airframe;

  /// No description provided for @motorOutputs.
  ///
  /// In zh, this message translates to:
  /// **'电机输出'**
  String get motorOutputs;

  /// No description provided for @portConfiguration.
  ///
  /// In zh, this message translates to:
  /// **'端口配置'**
  String get portConfiguration;

  /// No description provided for @remoteControl.
  ///
  /// In zh, this message translates to:
  /// **'遥控配置'**
  String get remoteControl;

  /// No description provided for @pidTuning.
  ///
  /// In zh, this message translates to:
  /// **'PID 调参'**
  String get pidTuning;

  /// No description provided for @calibration.
  ///
  /// In zh, this message translates to:
  /// **'传感器/外设配置'**
  String get calibration;

  /// No description provided for @safetySettings.
  ///
  /// In zh, this message translates to:
  /// **'安全设置'**
  String get safetySettings;

  /// No description provided for @flightLogs.
  ///
  /// In zh, this message translates to:
  /// **'飞行日志'**
  String get flightLogs;

  /// No description provided for @parametersDescription.
  ///
  /// In zh, this message translates to:
  /// **'{count} 个飞控参数'**
  String parametersDescription(int count);

  /// No description provided for @airframeDescription.
  ///
  /// In zh, this message translates to:
  /// **'按飞控类型配置 Copter 或 Plane/QuadPlane 机架'**
  String get airframeDescription;

  /// No description provided for @motorOutputsDescription.
  ///
  /// In zh, this message translates to:
  /// **'SERVO1-16 · 电机功能、反向和 PWM 范围'**
  String get motorOutputsDescription;

  /// No description provided for @portConfigurationDescription.
  ///
  /// In zh, this message translates to:
  /// **'{count} 个可用串口 · 修改后立即写入飞控'**
  String portConfigurationDescription(int count);

  /// No description provided for @remoteControlDescription.
  ///
  /// In zh, this message translates to:
  /// **'飞行模式、通道功能与摇杆校准'**
  String get remoteControlDescription;

  /// No description provided for @pidTuningDescription.
  ///
  /// In zh, this message translates to:
  /// **'根据当前机架自动显示控制器 · 修改后立即写入飞控'**
  String get pidTuningDescription;

  /// No description provided for @calibrationDescription.
  ///
  /// In zh, this message translates to:
  /// **'IMU、罗盘、GPS 与空速计'**
  String get calibrationDescription;

  /// No description provided for @safetySettingsDescription.
  ///
  /// In zh, this message translates to:
  /// **'围栏、解锁检查与电量保护'**
  String get safetySettingsDescription;

  /// No description provided for @flightLogsDescription.
  ///
  /// In zh, this message translates to:
  /// **'浏览飞控内存卡文件，并将日志或其他文件下载到电脑'**
  String get flightLogsDescription;

  /// No description provided for @parameterWriteFailures.
  ///
  /// In zh, this message translates to:
  /// **'{count} 项参数写入失败，已保留草稿'**
  String parameterWriteFailures(int count);

  /// No description provided for @parameterFileReadSummary.
  ///
  /// In zh, this message translates to:
  /// **'已读取 {parsed} 项：{staged} 项待写入，{unchanged} 项未变化，{unknown} 项未匹配'**
  String parameterFileReadSummary(
    int parsed,
    int staged,
    int unchanged,
    int unknown,
  );

  /// No description provided for @parameterFileReadFailed.
  ///
  /// In zh, this message translates to:
  /// **'读取参数文件失败：{error}'**
  String parameterFileReadFailed(String error);

  /// No description provided for @parametersSaved.
  ///
  /// In zh, this message translates to:
  /// **'已保存 {count} 项参数'**
  String parametersSaved(int count);

  /// No description provided for @parameterFileSaveFailed.
  ///
  /// In zh, this message translates to:
  /// **'保存参数文件失败：{error}'**
  String parameterFileSaveFailed(String error);

  /// No description provided for @unknown.
  ///
  /// In zh, this message translates to:
  /// **'未知'**
  String get unknown;

  /// No description provided for @notLocated.
  ///
  /// In zh, this message translates to:
  /// **'未定位'**
  String get notLocated;
}

class _MgLocalizationsDelegate extends LocalizationsDelegate<MgLocalizations> {
  const _MgLocalizationsDelegate();

  @override
  Future<MgLocalizations> load(Locale locale) {
    return SynchronousFuture<MgLocalizations>(lookupMgLocalizations(locale));
  }

  @override
  bool isSupported(Locale locale) =>
      <String>['en', 'zh'].contains(locale.languageCode);

  @override
  bool shouldReload(_MgLocalizationsDelegate old) => false;
}

MgLocalizations lookupMgLocalizations(Locale locale) {
  // Lookup logic when only language code is specified.
  switch (locale.languageCode) {
    case 'en':
      return MgLocalizationsEn();
    case 'zh':
      return MgLocalizationsZh();
  }

  throw FlutterError(
    'MgLocalizations.delegate failed to load unsupported locale "$locale". This is likely '
    'an issue with the localizations generation tool. Please file an issue '
    'on GitHub with a reproducible sample app and the gen-l10n configuration '
    'that was used.',
  );
}
