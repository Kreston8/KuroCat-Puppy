#ifndef PUPPY_SKIN_RES_H
#define PUPPY_SKIN_RES_H
// 小金毛贴图帧资源 ID（与 windows/skin/puppy_skin.rc 对应）
// 101-107：待机/抬头/低头/开心/狩猎/打字/眨眼
#define IDR_PUPPY_IDLE      101
#define IDR_PUPPY_LOOKUP    102
#define IDR_PUPPY_LOOKDOWN  103
#define IDR_PUPPY_HAPPY     104
#define IDR_PUPPY_HUNT      105
#define IDR_PUPPY_TYPE      106
#define IDR_PUPPY_BLINK     107
// 108-113：头部朝向网格的左列/右列（转头看鼠标用，可选，缺了自动沿用中列）
#define IDR_PUPPY_GAZE_L_UP   108
#define IDR_PUPPY_GAZE_L_MID  109
#define IDR_PUPPY_GAZE_L_DOWN 110
#define IDR_PUPPY_GAZE_R_UP   111
#define IDR_PUPPY_GAZE_R_MID  112
#define IDR_PUPPY_GAZE_R_DOWN 113

// 程序图标：idle.png 的狗头裁切生成的多尺寸 ICO（16-256）
#define IDI_PUPPY_APPICON   1
#endif
