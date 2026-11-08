import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/fleet/fleet_models.dart';

void main() {
  test('批量结果状态区分发送失败、拒绝、超时和仍在执行', () {
    // 链路层发送失败：命令没离开本机，不能折叠成"飞控拒绝"。
    expect(
      fleetBatchFailureStatus({
        'success': false,
        'error': {'code': -3, 'message': 'command send failed'},
      }),
      FleetBatchCommandStatus.sendFailed,
    );
    // 飞控明确拒绝。
    expect(
      fleetBatchFailureStatus({
        'success': false,
        'error': {'code': -16},
        'result_str': 'denied',
      }),
      FleetBatchCommandStatus.rejected,
    );
    // 没有 ACK。
    expect(
      fleetBatchFailureStatus({
        'success': false,
        'error': {'code': -6, 'message': 'command ack timeout'},
      }),
      FleetBatchCommandStatus.timeout,
    );
    // 飞控回过 IN_PROGRESS：还在执行，既不是拒绝也不是超时。
    expect(
      fleetBatchFailureStatus({
        'success': false,
        'error': {'code': -6},
        'result_str': 'in_progress',
        'in_progress': true,
      }),
      FleetBatchCommandStatus.inProgress,
    );
    expect(
      fleetBatchFailureStatus({
        'success': false,
        'error': {'code': -9},
      }),
      FleetBatchCommandStatus.error,
    );
  });

  test('执行中不计入失败数', () {
    final result = FleetBatchResult(
      action: FleetBatchAction.startMission,
      results: [
        FleetBatchCommandResult.success(vehicleId: 1, systemId: 1),
        FleetBatchCommandResult.failure(
          vehicleId: 2,
          systemId: 2,
          status: FleetBatchCommandStatus.inProgress,
          message: '飞控仍在执行该指令',
        ),
        FleetBatchCommandResult.failure(
          vehicleId: 3,
          systemId: 3,
          status: FleetBatchCommandStatus.notSent,
          message: '未回读到有效任务，未派发',
        ),
        FleetBatchCommandResult.failure(
          vehicleId: 4,
          systemId: 4,
          status: FleetBatchCommandStatus.rejected,
          message: '飞控拒绝了该指令',
        ),
      ],
    );
    expect(result.successCount, 1);
    expect(result.inProgressCount, 1);
    expect(result.notSentCount, 1);
    expect(result.failureCount, 1);
  });

  test('批量结果状态文案覆盖新增状态', () {
    expect(FleetBatchCommandStatus.sendFailed.label, '发送失败');
    expect(FleetBatchCommandStatus.inProgress.label, '执行中');
  });
}
