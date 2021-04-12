#include "x86.h"
#include "device.h"

int wait;

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE]; //buffer.
extern int bufferHead;
extern int bufferTail;

void GProtectFaultHandle(struct TrapFrame *tf);

void KeyboardHandle(struct TrapFrame *tf);

void syscallHandle(struct TrapFrame *tf);
void syscallWrite(struct TrapFrame *tf);
void syscallPrint(struct TrapFrame *tf);
void syscallRead(struct TrapFrame *tf);
void syscallGetChar(struct TrapFrame *tf);
void syscallGetStr(struct TrapFrame *tf);


void irqHandle(struct TrapFrame *tf) { // pointer tf = esp
	/*
	 * 中断处理程序
	 */
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%es"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%fs"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%gs"::"a"(KSEL(SEG_KDATA)));
	switch(tf->irq) {
		// TODO: 填好中断处理程序的调用
		case -1:
			break;
		case 0xd:
			GProtectFaultHandle(tf);
			break;	
		case 0x21:
			KeyboardHandle(tf);
			break;
		case 0x80:
			syscallHandle(tf);
			break;
		default:
			assert(0);
	}
}

void GProtectFaultHandle(struct TrapFrame *tf){
	assert(0);
	return;
}

void KeyboardHandle(struct TrapFrame *tf){
	uint32_t code = getKeyCode();
	//keyBuffer[bufferTail] = code;
	//bufferTail = (bufferTail + 1) % MAX_KEYBUFFER_SIZE;
	if(code == 0xe){ // 退格符
		// TODO: 要求只能退格用户键盘输入的字符串，且最多退到当行行首
		uint16_t data = 0 | (0x0c << 8);
		if(displayCol > 0){
			bufferTail--;
			if(bufferTail < 0)
				bufferTail = MAX_KEYBUFFER_SIZE - 1;
			displayCol--;
		}
		int pos = (80 * displayRow + displayCol) * 2;
                asm volatile("movw %0,(%1)"::"r"(data),"r"(pos+0xb8000));
	}else if(code == 0x1c){ // 回车符
		// TODO: 处理回车情况
		keyBuffer[bufferTail] = getChar(code);
		bufferTail = (bufferTail + 1) % MAX_KEYBUFFER_SIZE;
		displayRow++;
		displayCol = 0;
		if(displayRow == 25){
			displayRow = 24;
			displayCol = 0;
			scrollScreen();
		}
		wait = 1;
	}else if(code == 0x1d || code == 0x2a || code == 0x36 || code == 0x38 || code == 0x3a || code == 0x45 || code == 0x46){
		//不可打印字符
	}else if(code < 0x81){ // 正常字符
		keyBuffer[bufferTail] = getChar(code);
		bufferTail = (bufferTail + 1) % MAX_KEYBUFFER_SIZE;
		uint16_t data = 0;
		data = getChar(code) | (0x0c << 8);
		int pos = (80 * displayRow + displayCol) * 2;
		asm volatile("movw %0,(%1)"::"r"(data),"r"(pos+0xb8000));
		displayCol++;
		if(displayCol >= 80){
			displayRow++;
			displayCol = 0;
		}
		if(displayRow >= 25){
			displayRow = 24;
			scrollScreen();
		}
		// TODO: 注意输入的大小写的实现、不可打印字符的处理 // ?? how
	}
	updateCursor(displayRow, displayCol);
}

void syscallHandle(struct TrapFrame *tf) {
	switch(tf->eax) { // syscall number
		case 0:
			syscallWrite(tf);
			break; // for SYS_WRITE
		case 1:
			syscallRead(tf);
			break; // for SYS_READ
		default:break;
	}
}

void syscallWrite(struct TrapFrame *tf) {
	switch(tf->ecx) { // file descriptor
		case 0:
			syscallPrint(tf);
			break; // for STD_OUT
		default:break;
	}
}

void syscallPrint(struct TrapFrame *tf) {
	int sel = USEL(SEG_UDATA);//TODO: segment selector for user data, need further modification
	char *str = (char*)tf->edx;
	int size = tf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (i = 0; i < size; i++) {
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str+i));
		// TODO: 完成光标的维护和打印到显存
		if(character == '\n'){
			displayRow++;
			displayCol = 0;
			if(displayRow == 25){
				displayRow--;
				scrollScreen();
			}
		}
		else
		{
			data = character | (0x0f << 8);
			pos = (80 * displayRow + displayCol) * 2;
			asm volatile("movw %0,(%1)"::"r"(data),"r"(pos+0xb8000));
			displayCol++;
			if(displayCol == 80){
				displayRow++;
				displayCol = 0;
				if(displayRow == 25){
					displayRow = 24;
					displayCol = 0;
					scrollScreen();
				}
			}
		}
	}
	updateCursor(displayRow, displayCol);
}

void syscallRead(struct TrapFrame *tf){
	switch(tf->ecx){ //file descriptor
		case 0:
			syscallGetChar(tf);
			break; // for STD_IN
		case 1:
			syscallGetStr(tf);
			break; // for STD_STR
		default:break;
	}
}

void syscallGetChar(struct TrapFrame *tf){
	// TODO: 自由实现
	wait = 0;
	while(wait == 0){
		
	}
	tf->eax = keyBuffer[bufferHead];
	bufferHead++;
	if(bufferHead == MAX_KEYBUFFER_SIZE)
	{
		bufferHead = 0;
	}
	if(keyBuffer[bufferHead] == '\n'){
		bufferHead++;
		if(bufferHead == MAX_KEYBUFFER_SIZE)
			bufferHead = 0;
		wait = 0;
	}
}

void syscallGetStr(struct TrapFrame *tf){
	// TODO: 自由实现
	wait = 0;
	while(wait == 0){
	
	}
	
	
	int sel = USEL(SEG_UDATA);
 	int size = tf->ebx;

	//char str[MAX_KEYBUFFER_SIZE];
	//for(int i = 0;i < MAX_KEYBUFFER_SIZE;i++)
	//	str[i] = 0;

	char *str = (char*)tf->edx;
	int i = 0;
	char character = 0;
	asm volatile("movw %0,%%es"::"m"(sel));
	while(i < size - 1){
		character = keyBuffer[bufferHead];
		asm volatile("movb %0, %%es:(%1)"::"r"(character), "r"(str + i));
		bufferHead += 1;
		if (bufferHead == MAX_KEYBUFFER_SIZE) 
			bufferHead = 0;
		if (keyBuffer[bufferHead] == '\n') {
			wait = 0;
			bufferHead += 1;
			if (bufferHead == MAX_KEYBUFFER_SIZE)
				bufferHead = 0;
			break;
		}
		i += 1;
	}
	asm volatile("movb $0x00, %%es:(%0)"::"r"(str + i + 1));
	return;

	/*
	while(i < size){
		str[i] = keyBuffer[bufferHead];
		i++;
		bufferHead++;
		if(bufferHead == bufferTail){
			wait = 0;
			break;
		}
		if(bufferHead == MAX_KEYBUFFER_SIZE)
			bufferHead = 0;
	}
	tf->eax = (uint32_t)str;
	*/
}
