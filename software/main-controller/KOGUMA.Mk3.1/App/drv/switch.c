/*
 * switch.c
 * スイッチ S2 の読み取り
 *
 * ■ もとにしたもの
 * ToraNeko.Mk3 の App/drv/switch.c（ecd8e10）。KOGUMA.Mk3.1 で変えたところ：
 * 押したときの高さを逆にした（ToraNeko の SW1 は押すと High、KOGUMA の S2 は押すと Low）。
 *
 * S2 は PB3（CubeMX のラベル SW_USER）。外の 10kΩ（R31）で 3.3V に引き上げてあり、押すと GND につながる
 * （押すと Low、離すと High。KOGUMA.MkIIl の UI.c の Switch_Init のコメントから。回路図で確認）。
 * ピン名（SW_USER_GPIO_Port、SW_USER_Pin）は main.h に定義されている。
 * チャタリング（押した瞬間に High と Low が何回か入れ替わること）は、読む側（sys/ui.c）で取り除く。
 */
#include "drv/switch.h"

#include "main.h"

/* 今、スイッチが押されていれば true */
bool switch_is_pressed(void)
{
  if (HAL_GPIO_ReadPin(SW_USER_GPIO_Port, SW_USER_Pin) == GPIO_PIN_RESET) {
    return true;
  }
  return false;
}
