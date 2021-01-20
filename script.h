/* note: string/jumptable lengths do NOT count as an argument */
#define A_END 0
#define A_NUM 1
#define A_STR 2
#define A_JUMPOFFS 3
#define A_JUMPTABLE 4

#define ARGLISTMAX 8

typedef struct
{
    char *name;
    uint8_t arglist[ARGLISTMAX];
} cmdinfo_t;

const cmdinfo_t cmdtbl[] = {
/* 00 */{"EndScript", {A_END}},
/* 01 */{"ShowChara", {A_NUM,A_NUM,A_NUM,A_END}},
/* 02 */{"HideChara", {A_END}},
/* 03 */{"ShowTextBox", {A_STR,A_END}},
/* 04 */{"WaitTextBox", {A_END}},
/* 05 */{"Fade", {A_NUM,A_NUM,A_END}},
/* 06 */{"SetBG", {A_NUM,A_END}},
/* 07 */{"Wait", {A_NUM,A_END}},
/* 08 */{"Unk08", {A_NUM,A_NUM,A_NUM,A_NUM,A_NUM,A_NUM,A_NUM,A_END}},
/* 09 */{"ShowSelMenu09", {A_NUM,A_STR,A_END}},
/* 0a */{"JumpOnSelection", {A_JUMPTABLE,A_NUM,A_END}},
/* 0b */{"Unk0B", {A_NUM,A_NUM,A_END}},
/* 0c */{"Unk0C", {A_NUM,A_END}},
/* 0d */{"SetUpCounter", {A_NUM,A_END}},
/* 0e */{"Unk0E", {A_NUM,A_END}},
/* 0f */{"Unk0F", {A_NUM,A_END}},
/* 10 */{"DisableBG", {A_END}},
/* 11 */{"Unk11", {A_NUM,A_JUMPOFFS,A_END}},
/* 12 */{"Unk12", {A_NUM,A_NUM,A_END}},
/* 13 */{"ShowTextPrompt", {A_NUM,A_STR,A_END}},
/* 14 */{"Unk14", {A_NUM,A_NUM,A_END}},
/* 15 */{"Unk15", {A_END}},
/* 16 */{"DoorCloseTransition", {A_END}},
/* 17 */{"SineUnwipe", {A_NUM,A_END}},
/* 18 */{"SineWipe", {A_NUM,A_END}},
/* 19 */{"Unk19", {A_NUM,A_END}},
/* 1a */{"Unk1A", {A_END}},
/* 1b */{"Unk1B", {A_NUM,A_NUM,A_END}},
/* 1c */{"PlaySfx", {A_NUM,A_END}},
/* 1d */{"PlayMusic", {A_NUM,A_END}},
/* 1e */{"StopMusic", {A_END}},
/* 1f */{"ActivateUpCounter", {A_END}},
/* 20 */{"WaitUpCounter", {A_NUM,A_END}},
/* 21 */{"FadeOutMusic", {A_NUM,A_END}},
/* 22 */{"ShowSelMenu22", {A_NUM,A_STR,A_END}},
};