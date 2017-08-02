#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/system.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/string.h>
#include <linux/list.h>
#include <asm/delay.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/poll.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <linux/moduleparam.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <mach/hardware.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>

#include "gpio_i2c.h"
#include "nvp6114.h"
#include "video.h"
#include "audio.h"
#include "coax_protocol.h"
#include "motion.h"

//extern void sil9034_1080i60_init(void);
//extern void sil9034_1080i50_init(void);
//extern void sil9034_1080p30_init(void);

int chip_id[4];
int chip[4];
unsigned int vdec_mode = VDEC_NTSC; //0:ntsc, 1: pal
unsigned int vdec_cnt = 0;
unsigned int vdec_slave_addr[4] = {0x60, 0x62, 0x64, 0x66};
unsigned int outmode[4]={NVP6114_OUT_ALL_720P,NVP6114_OUT_ALL_720P,NVP6114_OUT_ALL_720P,NVP6114_OUT_ALL_720P};
vdec_input_videofmt stInputVideoFmt;
static nvp6114chn_t nvp6114_chn[MAX_CHN_CNT];
static unsigned char    nvp6114_buf[MAX_CHN_CNT];
DECLARE_WAIT_QUEUE_HEAD(nvp6114_wait);
struct task_struct      *nvp6114_task= NULL;
static int chg_flag = 0;
static int wake_up_flag = 0;
int reg_ec[4], reg_ed[4], reg_ee[4], reg_ef[4], reg_f0[4],reg_d8[4];
int map_ad_2708[]={0,1,2,3,4,5,6,7}; // according to  hardware version: DASBA V1.02
int map_ad_2708_vloss[]={7,6,5,4,3,2,1,0}; // according to  hardware version: DASBA V1.02
//int map_ad_2708[]={0,1,4,5,2,3,6,7}; // according to  hardware version: DALBA V1.02
//int map_ad_2708_vloss[]={7,6,3,2,5,4,1,0}; // according to  hardware version: DALBA V1.02
int map_ad_2704[]={0,1,2,3}; // according to  hardware version: DTLAA V1.01
int map_ad_2704_vloss[]={3,2,1,0}; // according to  hardware version: DTLAA V1.01
//int map_ad_2704[]={3,2,1,0}; // according to  hardware version: DTLAA V1.01
//int map_ad_2704_vloss[]={0,1,2,3}; // according to  hardware version: DTLAA V1.01
//int map_ad_2704[]={0,1,2,3}; // according to  hardware version: DASBA V1.02
//int map_ad_2704_vloss[]={3,2,1,0}; // according to  hardware version: DASBA V1.02
struct mutex mutex;
static int enable_thread_flag = 0;
static int enable_thread_flag_cf = 0;
static int soft_reset[4] = {0xFF, 0xFF, 0xFF, 0xFF};

//unsigned int outmode=NVP6114_OUT_ALL_720P;
module_param_array(outmode,  uint, (void *)4, S_IRUGO);
module_param(vdec_mode, uint, S_IRUGO);

int check_id(unsigned int dec)
{
	int ret;
	gpio_i2c_write(dec, 0xFF, 0x01);
	ret = gpio_i2c_read(dec, 0xf4);
	return ret;
}

int vdec_open(struct inode * inode, struct file * file)
{
	return 0;
} 

int vdec_close(struct inode * inode, struct file * file)
{
	return 0;
}


void video_fmt_det(vdec_input_videofmt *pvideofmt)
{
	int i;
	for(i=0;i<vdec_cnt;i++)
	{
		gpio_i2c_write(vdec_slave_addr[i], 0xFF, 0x01);
		reg_ec[i] = gpio_i2c_read(vdec_slave_addr[i], 0xEC);
		reg_ed[i] = gpio_i2c_read(vdec_slave_addr[i], 0xED);
		reg_ee[i] = gpio_i2c_read(vdec_slave_addr[i], 0xEE);
		reg_ef[i] = gpio_i2c_read(vdec_slave_addr[i], 0xEF);
		reg_f0[i] = gpio_i2c_read(vdec_slave_addr[i], 0xF0);
		reg_d8[i] = gpio_i2c_read(vdec_slave_addr[i], 0xD8);
		//printk("0xEC=%x 0xED=%x 0xEE=%x 0xEF=%x 0xF0=%x 0xD8=%x\n", reg_ec[i], reg_ed[i],reg_ee[i],reg_ef[i],reg_f0[i],reg_d8[i]);
	}
	for(i=0;i<(4*vdec_cnt);i++) 
	{
	if(VDEC_NTSC == vdec_mode)
	{
		if(outmode[i/4] == NVP6114_OUT_ALL_960H)
		{
			if(((reg_ef[i/4]>>(i%4*2) &0x03)==0x02) && (reg_ec[i/4]>>(i%4) &0x01)==0x01)
			{
				pvideofmt->getvideofmt[i] = VIDEO_TYPE_960H30;
				pvideofmt->chn[i] = i;
				//printk("ch[%d] video input fmt[SD PAL]\n", i);
			}
			else if(((reg_ef[i/4]>>(i%4*2) &0x03)==0x03) && (reg_ec[i/4]>>(i%4) &0x01)==0x01)
			{
				pvideofmt->getvideofmt[i] = VIDEO_TYPE_960H30;
				pvideofmt->chn[i] = i;
				//printk("ch[%d] video input fmt[SD NTSC]\n", i);
			}
			else
			{
				if(0 == ((reg_ee[i/4]>>(i%4+4)) & 0x01))
				{
					if(((reg_ed[i/4]>>(i%4+4)) & 0x01) &&((reg_f0[i/4]>>(i%4*2) &0x03)==0x03))
					{
						pvideofmt->getvideofmt[i] = VIDEO_TYPE_720P30;
						pvideofmt->chn[i] = i;
						//printk("ch[%d] video input fmt[AHD 30P]\n", i);
					}
					//else
				}
				else //if((reg_ee>>(i+4)) & 0x01)
				{
					if(0 == ((reg_ed[i/4]>>(i%4+4)) & 0x01) &&((reg_f0[i/4]>>(i%4*2)&0x03)==0x03))
					{
						pvideofmt->getvideofmt[i] = VIDEO_TYPE_720P30;
						pvideofmt->chn[i] = i;
						//printk("ch[%d] video input fmt[AHD 25P]\n", i);
					}	
				}
			}
		}
		else
		{
			if((reg_ec[i/4]>>(i%4+4)) & 0x01)
			{
				pvideofmt->getvideofmt[i] = VIDEO_TYPE_960H30;
				pvideofmt->chn[i] = i;
				//printk("ch[%d] video input fmt[SD]\n", i); 
			}
			else
			{
				if(0 == ((reg_ee[i/4]>>(i%4+4)) & 0x01))
				{
					if(((reg_ed[i/4]>>(i%4+4)) & 0x01) &&((reg_f0[i/4]>>(i%4*2) &0x03)==0x03))
					{
						pvideofmt->getvideofmt[i] = VIDEO_TYPE_720P30;
						pvideofmt->chn[i] = i;
						//printk("ch[%d] video input fmt[AHD 30P]\n", i);
					}
					//else
				}
				else //if((reg_ee>>(i+4)) & 0x01)
				{
					if(0 == ((reg_ed[i/4]>>(i%4+4)) & 0x01) &&((reg_f0[i/4]>>(i%4*2)&0x03)==0x03))
					{
						pvideofmt->getvideofmt[i] = VIDEO_TYPE_720P30;
						pvideofmt->chn[i] = i;
						//printk("ch[%d] video input fmt[AHD 25P]\n", i);
					}	
				}
			}
		}
	}
	else
	{
		if(outmode[i/4] == NVP6114_OUT_ALL_960H)
		{
			if(((reg_ef[i/4]>>(i%4*2) &0x03)==0x02) && (reg_ec[i/4]>>(i%4) &0x01)==0x01)
			{
				pvideofmt->getvideofmt[i] = VIDEO_TYPE_960H25;
				pvideofmt->chn[i] = i;
				//printk("ch[%d] video input fmt[SD PAL]\n", i);
			}
			else if(((reg_ef[i/4]>>(i%4*2) &0x03)==0x03) && (reg_ec[i/4]>>(i%4) &0x01)==0x01)
			{
				pvideofmt->getvideofmt[i] = VIDEO_TYPE_960H25;
				pvideofmt->chn[i] = i;
				//printk("ch[%d] video input fmt[SD NTSC]\n", i);
			}
			else
			{
				if(0 == ((reg_ee[i/4]>>(i%4+4)) & 0x01))
				{
					if(((reg_ed[i/4]>>(i%4+4)) & 0x01) &&((reg_f0[i/4]>>(i%4*2) &0x03)==0x03))
					{
						pvideofmt->getvideofmt[i] = VIDEO_TYPE_720P25;
						pvideofmt->chn[i] = i;
						//printk("ch[%d] video input fmt[AHD 30P]\n", i);
					}
					//else
				}
				else //if((reg_ee>>(i+4)) & 0x01)
				{
					if(0 == ((reg_ed[i/4]>>(i%4+4)) & 0x01) &&((reg_f0[i/4]>>(i%4*2)&0x03)==0x03))
					{
						pvideofmt->getvideofmt[i] = VIDEO_TYPE_720P25;
						pvideofmt->chn[i] = i;
						//printk("ch[%d] video input fmt[AHD 25P]\n", i);
					}	
				}
			}
		}
		else
		{
			if((reg_ec[i/4]>>(i%4+4)) & 0x01)
			{
				pvideofmt->getvideofmt[i] = VIDEO_TYPE_960H25;
				pvideofmt->chn[i] = i;
				//printk("ch[%d] video input fmt[SD]\n", i); 
			}
			else
			{
				if(0 == ((reg_ee[i/4]>>(i%4+4)) & 0x01))
				{
					if(((reg_ed[i/4]>>(i%4+4)) & 0x01) &&((reg_f0[i/4]>>(i%4*2) &0x03)==0x03))
					{
						pvideofmt->getvideofmt[i] = VIDEO_TYPE_720P25;
						pvideofmt->chn[i] = i;
						//printk("ch[%d] video input fmt[AHD 30P]\n", i);
					}
					//else
				}
				else //if((reg_ee>>(i+4)) & 0x01)
				{
					if(0 == ((reg_ed[i/4]>>(i%4+4)) & 0x01) &&((reg_f0[i/4]>>(i%4*2)&0x03)==0x03))
					{
						pvideofmt->getvideofmt[i] = VIDEO_TYPE_720P25;
						pvideofmt->chn[i] = i;
						//printk("ch[%d] video input fmt[AHD 25P]\n", i);
					}	
				}
			}
		}
	}
	}
}

static int nvp6114_init2(void)
{
	int i;
	nvp6114chn_t* p_nvp6114_chn = NULL;

	for(i= 0;i<vdec_cnt*4;i++)
	{
		p_nvp6114_chn = &nvp6114_chn[i];
		p_nvp6114_chn->chn_chg_flag = 0;
		stInputVideoFmt.getvideofmt[i] = 0xFF;
		p_nvp6114_chn->video_type = 0xFF;
		p_nvp6114_chn->video_type_old = 0xFF;
	}
	
	return 0;	
}

static int nvp6114_init(unsigned int vdec_mode, unsigned int *outmode)
{
	int i;
	nvp6114chn_t* p_nvp6114_chn = NULL;

	for(i= 0;i<vdec_cnt*4;i++)
	{
		p_nvp6114_chn = &nvp6114_chn[i];
		p_nvp6114_chn->chn_chg_flag = 0;
		switch(outmode[i/4])
		{
			case NVP6114_OUT_ALL_720P:
				if(VDEC_NTSC == vdec_mode)
				{
					stInputVideoFmt.getvideofmt[i] = VIDEO_TYPE_720P30;
					p_nvp6114_chn->video_type = VIDEO_TYPE_720P30;
					p_nvp6114_chn->video_type_old = p_nvp6114_chn->video_type;
				}
				else
				{
					stInputVideoFmt.getvideofmt[i] = VIDEO_TYPE_720P25;
					p_nvp6114_chn->video_type = VIDEO_TYPE_720P25;
					p_nvp6114_chn->video_type_old = p_nvp6114_chn->video_type;
				}
				break;
			case NVP6114_OUT_ALL_960H:
				if(VDEC_NTSC == vdec_mode)
				{
					stInputVideoFmt.getvideofmt[i] = VIDEO_TYPE_960H30;
					p_nvp6114_chn->video_type = VIDEO_TYPE_960H30;
					p_nvp6114_chn->video_type_old = p_nvp6114_chn->video_type;
				}
				else
				{
					stInputVideoFmt.getvideofmt[i] = VIDEO_TYPE_960H25;
					p_nvp6114_chn->video_type = VIDEO_TYPE_960H25;
					p_nvp6114_chn->video_type_old = p_nvp6114_chn->video_type;
				}
				break;
			case NVP6114_OUT_2X960_2X720P:
				if(VDEC_NTSC == vdec_mode)
				{
					if(i%4 < 2)
					{
						stInputVideoFmt.getvideofmt[i] = VIDEO_TYPE_960H30;
						p_nvp6114_chn->video_type = VIDEO_TYPE_960H30;
						p_nvp6114_chn->video_type_old = p_nvp6114_chn->video_type;
					}
					else
					{
						stInputVideoFmt.getvideofmt[i] = VIDEO_TYPE_720P30;
						p_nvp6114_chn->video_type = VIDEO_TYPE_720P30;
						p_nvp6114_chn->video_type_old = p_nvp6114_chn->video_type;
					}
				}
				else
				{
					if(i%4 < 2)
					{
						stInputVideoFmt.getvideofmt[i] = VIDEO_TYPE_960H25;
						p_nvp6114_chn->video_type = VIDEO_TYPE_960H25;
						p_nvp6114_chn->video_type_old = p_nvp6114_chn->video_type;
					}
					else
					{
						stInputVideoFmt.getvideofmt[i] = VIDEO_TYPE_720P25;
						p_nvp6114_chn->video_type = VIDEO_TYPE_720P25;
						p_nvp6114_chn->video_type_old = p_nvp6114_chn->video_type;
					}
				}
				break;
			case NVP6114_OUT_2X720P_2X960H:
				if(VDEC_NTSC == vdec_mode)
				{
					if(i%4 < 2)
					{
						stInputVideoFmt.getvideofmt[i] = VIDEO_TYPE_720P30;
						p_nvp6114_chn->video_type = VIDEO_TYPE_720P30;
						p_nvp6114_chn->video_type_old = p_nvp6114_chn->video_type;
					}
					else
					{
						stInputVideoFmt.getvideofmt[i] = VIDEO_TYPE_960H30;
						p_nvp6114_chn->video_type = VIDEO_TYPE_960H30;
						p_nvp6114_chn->video_type_old = p_nvp6114_chn->video_type;
					}
				}
				else
				{
					if(i%4 < 2)
					{
						stInputVideoFmt.getvideofmt[i] = VIDEO_TYPE_720P25;
						p_nvp6114_chn->video_type = VIDEO_TYPE_720P25;
						p_nvp6114_chn->video_type_old = p_nvp6114_chn->video_type;
					}
					else
					{
						stInputVideoFmt.getvideofmt[i] = VIDEO_TYPE_960H25;
						p_nvp6114_chn->video_type = VIDEO_TYPE_960H25;
						p_nvp6114_chn->video_type_old = p_nvp6114_chn->video_type;
					}
				}
				break;
			default:
				if(VDEC_NTSC == vdec_mode)
				{
					stInputVideoFmt.getvideofmt[i] = VIDEO_TYPE_960H30;
					p_nvp6114_chn->video_type = VIDEO_TYPE_960H30;
					p_nvp6114_chn->video_type_old = p_nvp6114_chn->video_type;
				}
				else
				{
					stInputVideoFmt.getvideofmt[i] = VIDEO_TYPE_720P25;
					p_nvp6114_chn->video_type = VIDEO_TYPE_720P25;
					p_nvp6114_chn->video_type_old = p_nvp6114_chn->video_type;
				}
				break;
		}
	}
	
	return 0;	
}

static int nvp6114_thread(void *unused)
{
	int i;
	int value = 0;
	int *map_chn = NULL;

	while (!kthread_should_stop()) 
	{
	//// 检测和上报
	if(!chg_flag && enable_thread_flag && enable_thread_flag_cf) //避免到读到的是不对应的buf值，加enable信号是为避免上层未配置制式已经上报默认制式下的检测结果
	{
		mutex_lock(&mutex);
		video_fmt_det(&stInputVideoFmt);
		for(i=0;i<vdec_cnt*2;i++)
		{
			if( vdec_cnt == 1 )
			{
				map_chn = map_ad_2704;
			}
			else if( vdec_cnt == 2 )
			{
				map_chn = map_ad_2708;
			}
			
			nvp6114_chn[i*2].chn_chg_flag = 0;
			nvp6114_chn[i*2 + 1].chn_chg_flag = 0;
			if( stInputVideoFmt.getvideofmt[i*2] != nvp6114_chn[i*2].video_type && ( (reg_d8[i/2] >> (i%2*2 +1) & 0x01) == 0x01 || nvp6114_chn[i*2].video_type == 0xff ) ) //绗竴娆″浐瀹氫笂鎶ワ紝瀹炵幇鐢ㄦ埛淇濆瓨鍙傛暟鍚庝慨鏀圭嚎浠嶈兘姝ｅ父鏄剧ず锛屾帴濂界嚎鐨勯兘涓婃姤锛屼笉鎺ョ嚎鐨勪笉涓婃姤
			{

				chip[i/2] = 1;
				nvp6114_chn[i*2].video_type_old = nvp6114_chn[i*2].video_type;
				nvp6114_chn[i*2].video_type = stInputVideoFmt.getvideofmt[i*2];
				nvp6114_chn[i*2+1].video_type_old = nvp6114_chn[i*2+1].video_type;
				nvp6114_chn[i*2+1].video_type = stInputVideoFmt.getvideofmt[i*2];
				nvp6114_chn[i*2].chn_chg_flag = 1;
				nvp6114_chn[i*2+1].chn_chg_flag = 1;
				wake_up_flag = 1;
				value = (nvp6114_chn[i*2].video_type_old & 0x0f) << 4;
				value |= (nvp6114_chn[i*2].video_type & 0x0f);
				nvp6114_buf[map_chn[i*2]] = value; //映射对外VI通道号
				nvp6114_buf[map_chn[i*2+1]] = value;
			}
			else if( stInputVideoFmt.getvideofmt[i*2+1] != nvp6114_chn[i*2+1].video_type && ( (reg_d8[i/2] >> (i%2*2) & 0x01) == 0x01 || nvp6114_chn[i*2+1].video_type == 0xff ) )
			{
				chip[i/2] = 1;
				nvp6114_chn[i*2].video_type_old = nvp6114_chn[i*2].video_type;
				nvp6114_chn[i*2].video_type = stInputVideoFmt.getvideofmt[i*2+1];
				nvp6114_chn[i*2+1].video_type_old = nvp6114_chn[i*2+1].video_type;
				nvp6114_chn[i*2+1].video_type = stInputVideoFmt.getvideofmt[i*2+1];
				nvp6114_chn[i*2].chn_chg_flag = 1;
				nvp6114_chn[i*2+1].chn_chg_flag = 1;
				wake_up_flag = 1;
				value = (nvp6114_chn[i*2+1].video_type_old & 0x0f) << 4;
				value |= (nvp6114_chn[i*2+1].video_type & 0x0f);
				nvp6114_buf[map_chn[i*2]] = value;
				nvp6114_buf[map_chn[i*2+1]] = value;
			}
			else//这里需要更新，以免上传上次变化的通道
			{
				nvp6114_chn[i*2].video_type_old = nvp6114_chn[i*2].video_type;
				value = (nvp6114_chn[i*2].video_type_old & 0x0f) << 4;
				value |= (nvp6114_chn[i*2].video_type & 0x0f);
				nvp6114_buf[map_chn[i*2]] = value;
				nvp6114_chn[i*2+1].video_type_old = nvp6114_chn[i*2+1].video_type;
				value = (nvp6114_chn[i*2+1].video_type_old & 0x0f) << 4;
				value |= (nvp6114_chn[i*2+1].video_type & 0x0f);
				nvp6114_buf[map_chn[i*2+1]] = value;
			}
		}


#if 1
		//处理：切换输出模式
		for(i=0;i<vdec_cnt;i++)
		{
			switch(outmode[i])
			{
				case NVP6114_OUT_ALL_720P: //由于第一次是0xFF,因此默认4X720p这里需要判断输入分辨率
					if( (nvp6114_chn[i*4].chn_chg_flag == 1 && nvp6114_chn[i*4+2].chn_chg_flag == 1)
					&& (nvp6114_chn[i*4].video_type == VIDEO_TYPE_960H25 || nvp6114_chn[i*4].video_type == VIDEO_TYPE_960H30)
					&& (nvp6114_chn[i*4+2].video_type == VIDEO_TYPE_960H25 || nvp6114_chn[i*4+2].video_type == VIDEO_TYPE_960H30) )
					{
						outmode[i] = NVP6114_OUT_ALL_960H;
						soft_reset[i] = 0x0F;
						printk("Current NVP6114_OUT_ALL_720P, switch to NVP6114_OUT_ALL_960H\n");
					}
					else if( (nvp6114_chn[i*4].chn_chg_flag == 1 && nvp6114_chn[i*4+2].chn_chg_flag == 1)
					&& (nvp6114_chn[i*4].video_type == VIDEO_TYPE_960H25 || nvp6114_chn[i*4].video_type == VIDEO_TYPE_960H30)
					&& (nvp6114_chn[i*4+2].video_type == VIDEO_TYPE_720P25 || nvp6114_chn[i*4+2].video_type == VIDEO_TYPE_720P30) )
					{
						outmode[i] = NVP6114_OUT_2X960_2X720P;
						soft_reset[i] = 0x03;
						printk("Current NVP6114_OUT_ALL_720P, switch to NVP6114_OUT_2X960_2X720P\n");
					}
					else if( (nvp6114_chn[i*4].chn_chg_flag == 1 && nvp6114_chn[i*4+2].chn_chg_flag == 1)
					&& (nvp6114_chn[i*4].video_type == VIDEO_TYPE_720P25 || nvp6114_chn[i*4].video_type == VIDEO_TYPE_720P30)
					&& (nvp6114_chn[i*4+2].video_type == VIDEO_TYPE_960H25 || nvp6114_chn[i*4+2].video_type == VIDEO_TYPE_960H30) )
					{
						outmode[i] = NVP6114_OUT_2X720P_2X960H;
						soft_reset[i] = 0x0C;
						printk("Current NVP6114_OUT_ALL_720P, switch to NVP6114_OUT_2X720P_2X960H\n");
					}
					else if( (nvp6114_chn[i*4].chn_chg_flag == 1 && nvp6114_chn[i*4+2].chn_chg_flag == 0)
					&& (nvp6114_chn[i*4].video_type == VIDEO_TYPE_960H25 || nvp6114_chn[i*4].video_type == VIDEO_TYPE_960H30) )
					{
						outmode[i] = NVP6114_OUT_2X960_2X720P;
						soft_reset[i] = 0x03;
						printk("Current NVP6114_OUT_ALL_720P, switch to NVP6114_OUT_2X960_2X720P\n");
					}
					else if( (nvp6114_chn[i*4].chn_chg_flag == 0 && nvp6114_chn[i*4+2].chn_chg_flag == 1)
					&& (nvp6114_chn[i*4+2].video_type == VIDEO_TYPE_960H25 || nvp6114_chn[i*4+2].video_type == VIDEO_TYPE_960H30) )
					{
						outmode[i] = NVP6114_OUT_2X720P_2X960H;
						soft_reset[i] = 0x0C;
						printk("Current NVP6114_OUT_ALL_720P, switch to NVP6114_OUT_2X720P_2X960H\n");
					}
					else
					{
						//printk("Current NVP6114_OUT_ALL_720P mode, not use to change.\n");
					}
				break;

				case NVP6114_OUT_2X720P_2X960H:
					if( (nvp6114_chn[i*4].chn_chg_flag == 1 && nvp6114_chn[i*4+1].chn_chg_flag == 1)
					&& (nvp6114_chn[i*4+2].chn_chg_flag == 1 && nvp6114_chn[i*4+3].chn_chg_flag == 1))
					{
						outmode[i] = NVP6114_OUT_2X960_2X720P;
						soft_reset[i] = 0x0F;
						printk("Current NVP6114_OUT_2X720P_2X960H, switch to NVP6114_OUT_2X960_2X720P\n");
					}
					else if( (nvp6114_chn[i*4].chn_chg_flag == 1 && nvp6114_chn[i*4+1].chn_chg_flag == 1)
					&& (nvp6114_chn[i*4+2].chn_chg_flag == 0 && nvp6114_chn[i*4+3].chn_chg_flag == 0))
					{
						outmode[i] = NVP6114_OUT_ALL_960H;
						soft_reset[i] = 0x03;
						printk("Current NVP6114_OUT_2X720P_2X960H, switch to NVP6114_OUT_ALL_960H\n");
					}
					else if( (nvp6114_chn[i*4].chn_chg_flag == 0 && nvp6114_chn[i*4+1].chn_chg_flag == 0)
					&& (nvp6114_chn[i*4+2].chn_chg_flag == 1 && nvp6114_chn[i*4+3].chn_chg_flag == 1))
					{
						outmode[i] = NVP6114_OUT_ALL_720P;
						soft_reset[i] = 0x0C;
						printk("Current NVP6114_OUT_2X720P_2X960H, switch to NVP6114_OUT_ALL_720P\n");
					}
					else
					{
						//printk("Current NVP6114_OUT_2X720P_2X960H mode, not use to change.\n");
					}
				break;

				case NVP6114_OUT_2X960_2X720P:
					if( (nvp6114_chn[i*4].chn_chg_flag == 1 && nvp6114_chn[i*4+1].chn_chg_flag == 1)
					&& (nvp6114_chn[i*4+2].chn_chg_flag == 1 && nvp6114_chn[i*4+3].chn_chg_flag == 1))
					{
						outmode[i] = NVP6114_OUT_2X720P_2X960H;
						soft_reset[i] = 0x0F;
						printk("Current NVP6114_OUT_2X960_2X720P, switch to NVP6114_OUT_2X720P_2X960H\n");
					}
					else if( (nvp6114_chn[i*4].chn_chg_flag == 1 && nvp6114_chn[i*4+1].chn_chg_flag == 1)
					&& (nvp6114_chn[i*4+2].chn_chg_flag == 0 && nvp6114_chn[i*4+3].chn_chg_flag == 0))
					{
						outmode[i] = NVP6114_OUT_ALL_720P;
						soft_reset[i] = 0x03;
						printk("Current NVP6114_OUT_2X960_2X720P, switch to NVP6114_OUT_ALL_720P\n");
					}
					else if( (nvp6114_chn[i*4].chn_chg_flag == 0 && nvp6114_chn[i*4+1].chn_chg_flag == 0)
					&& (nvp6114_chn[i*4+2].chn_chg_flag == 1 && nvp6114_chn[i*4+3].chn_chg_flag == 1))
					{
						outmode[i] = NVP6114_OUT_ALL_960H;
						soft_reset[i] = 0x0C;
						printk("Current NVP6114_OUT_2X960_2X720P, switch to NVP6114_OUT_ALL_960H\n");
					}
					else
					{
						//printk("Current NVP6114_OUT_2X960_2X720P mode, not use to change.\n");
					}
				break;

				case NVP6114_OUT_ALL_960H:
					if( (nvp6114_chn[i*4].chn_chg_flag == 1 && nvp6114_chn[i*4+1].chn_chg_flag == 1)
					&& (nvp6114_chn[i*4+2].chn_chg_flag == 1 && nvp6114_chn[i*4+3].chn_chg_flag == 1))
					{
						
						outmode[i] = NVP6114_OUT_ALL_720P;
						soft_reset[i] = 0x0F;
						printk("Current NVP6114_OUT_ALL_960H, switch to NVP6114_OUT_ALL_720P\n");
					}
					else if( (nvp6114_chn[i*4].chn_chg_flag == 1 && nvp6114_chn[i*4+1].chn_chg_flag == 1)
					&& (nvp6114_chn[i*4+2].chn_chg_flag == 0 && nvp6114_chn[i*4+3].chn_chg_flag == 0))
					{
						outmode[i] = NVP6114_OUT_2X720P_2X960H;
						soft_reset[i] = 0x03;
						printk("Current NVP6114_OUT_ALL_960H, switch to NVP6114_OUT_2X720P_2X960H\n");
					}
					else if( (nvp6114_chn[i*4].chn_chg_flag == 0 && nvp6114_chn[i*4+1].chn_chg_flag == 0)
					&& (nvp6114_chn[i*4+2].chn_chg_flag == 1 && nvp6114_chn[i*4+3].chn_chg_flag == 1))
					{
						outmode[i] = NVP6114_OUT_2X960_2X720P;
						soft_reset[i] = 0x0C;
						printk("Current NVP6114_OUT_ALL_960H, switch to NVP6114_OUT_2X960_2X720P\n");
					}
					else
					{
						//printk("Current NVP6114_OUT_ALL_960H mode, not use to change.\n");
					}
				break;

				default:
				break;
			}
		}
#endif

		nvp6114_init(vdec_mode, outmode); //屏蔽拔线后对应stInputVideoFmt.getvideofmt值仍在，导致另一口插入时反复来回切换的问题，这里必须在输出模式切换后做
		if(1 == wake_up_flag && 0==chg_flag)
		{
			enable_thread_flag_cf = 0;
			wake_up_flag = 0;
			chg_flag = 1;			
			wake_up(&nvp6114_wait);
		}

		mutex_unlock(&mutex);
	}
		msleep(300);
	}
	return 0;
}


static ssize_t nvp6114_read(struct file *filp,char *buffer,size_t count,loff_t *ppos) {

	if(count > MAX_CHN_CNT)
		count=MAX_CHN_CNT;
	if (!chg_flag) {
		if (filp->f_flags & O_NONBLOCK)
		    return -EAGAIN;
		else
		    wait_event_interruptible(nvp6114_wait, chg_flag == 1);
	    }

	if (copy_to_user( buffer,&nvp6114_buf, sizeof(nvp6114_buf)))
		return -EFAULT;
	mutex_lock(&mutex);
	chg_flag = 0;
	memset(nvp6114_buf, 0x0,  sizeof(nvp6114_buf));
	mutex_unlock(&mutex);

	return (sizeof(nvp6114_buf));

}

static unsigned int nvp6114_poll( struct file *file, struct poll_table_struct *wait)
{
    unsigned int mask = 0;
    poll_wait(file, &nvp6114_wait, wait);
    if (chg_flag)
    {
        mask |= POLLIN | POLLRDNORM;
    }
    return mask;
}
extern void software_reset(int i);
extern void nvp6114_vafe_reset(int i);
long vdec_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned int __user *argp = (unsigned int __user *)arg;	
	unsigned int on;
	unsigned int vloss=0;
	unsigned int vloss_map=0;
	unsigned int *vloss_map_chn=0;
	unsigned long ptz_ch;
	unsigned int value;
	unsigned int i;
	unsigned int sens[16];
	vdec_video_mode vmode;
	vdec_video_adjust v_adj;
	NVP1918_Audio_Init_Val audio_init_val;

	switch (cmd)
	{
		case IOC_VDEC_GET_VIDEO_LOSS:
		{
			if( vdec_cnt == 1 )
			{
				vloss_map_chn = map_ad_2704_vloss;
			}
			else if( vdec_cnt == 2 )
			{
				vloss_map_chn = map_ad_2708_vloss;
			}
			
			mutex_lock(&mutex);
			for(i=0;i<vdec_cnt;i++)
			{
				//gpio_i2c_write(vdec_slave_addr[1], 0xFF, 0x01);
				//vloss=gpio_i2c_read(vdec_slave_addr[1], 0xD8)<<4;
				gpio_i2c_write(vdec_slave_addr[i], 0xFF, 0x01);
				vloss |= gpio_i2c_read(vdec_slave_addr[i], 0xD8) << i*4;
			}
			mutex_unlock(&mutex);
		
			//printk("IOC_VDEC_GET_VIDEO_LOSS vloss: %#x\n", vloss);
			for(i=0;i<vdec_cnt*4;i++)
			{
				vloss_map |= (vloss >> i & 0x01) << vloss_map_chn[i];
			}
			//printk("IOC_VDEC_GET_VIDEO_LOSS vloss_map: %#x\n", vloss_map);
			if(copy_to_user(argp, &vloss_map, sizeof(unsigned int)))
				printk("IOC_VDEC_GET_VIDEO_LOSS error\n");
		}
		break;

		case IOC_VDEC_SET_VIDEO_OUTMODE:
		mutex_lock(&mutex);
		for(i=0;i<vdec_cnt;i++)
		{
			if(1 == chip[i])
			{
				if(outmode[i] == NVP6114_OUT_ALL_720P)
				{
					gpio_i2c_write(vdec_slave_addr[i], 0xFF, 0x03); // clear sd/AHD status
					gpio_i2c_write(vdec_slave_addr[i], 0x0B, soft_reset[i]);
					gpio_i2c_write(vdec_slave_addr[i], 0x0B, 0x0);
					nvp6114_960H_setting(vdec_mode&0x01, 0x00, i);
					printk("set chip%d to NVP6114_OUT_ALL_720P success\n",i);
				}
				else if(outmode[i] == NVP6114_OUT_ALL_960H)
				{
					gpio_i2c_write(vdec_slave_addr[i], 0xFF, 0x03);
					gpio_i2c_write(vdec_slave_addr[i], 0x0B, soft_reset[i]);
					gpio_i2c_write(vdec_slave_addr[i], 0x0B, 0x0);
					nvp6114_960H_setting(vdec_mode&0x01, 0x0F, i);
					printk("set chip%d to NVP6114_OUT_ALL_960H success\n",i);
				}
				else if(outmode[i] == NVP6114_OUT_2X960_2X720P)
				{
					gpio_i2c_write(vdec_slave_addr[i], 0xFF, 0x03);
					gpio_i2c_write(vdec_slave_addr[i], 0x0B, soft_reset[i]);
					gpio_i2c_write(vdec_slave_addr[i], 0x0B, 0x0);
					nvp6114_960H_setting(vdec_mode&0x01, 0x03, i);
					//nvp6114_hybrid_mode_1(vdec_mode&0x01);
					printk("set chip%d to NVP6114_OUT_2X960_2X720P success\n",i);
				}
				else if(outmode[i] == NVP6114_OUT_2X720P_2X960H)
				{
					gpio_i2c_write(vdec_slave_addr[i], 0xFF, 0x03);
					gpio_i2c_write(vdec_slave_addr[i], 0x0B, soft_reset[i]);
					gpio_i2c_write(vdec_slave_addr[i], 0x0B, 0x0);
					nvp6114_960H_setting(vdec_mode&0x01, 0x0C, i);
					//nvp6114_hybrid_mode_2(vdec_mode&0x01);
					printk("set chip%d to NVP6114_OUT_2X720P_2X960H success\n",i);
				}
				else
				{
					printk("err!!! not support outmode: %d\n", outmode[i]);
				}

				if(1 == vdec_cnt)
				{
					//software_reset(i);
					nvp6114_vafe_reset(i);
				}
				chip[i] = 0;
			}
		}
		//chg_flag = 0;
		enable_thread_flag_cf = 1;
		mutex_unlock(&mutex);
		break;

		case IOC_VDEC_SET_VIDEO_MODE:
//		mutex_lock(&mutex);
    		if (copy_from_user(&vmode, argp, sizeof(vdec_video_mode)))
			return -1;
		vdec_mode = vmode.mode;
		enable_thread_flag = 1;
		enable_thread_flag_cf = 1;
		msleep(3000);
		//mdelay(300);
		mutex_lock(&mutex);
		for(i=0;i<vdec_cnt;i++)
		{
			if(vdec_mode == VDEC_NTSC)
				nvp6114_720p_30fps(i, 0x0);
			else
				nvp6114_720p_25fps(i, 0x0);

			if(outmode[i] == NVP6114_OUT_ALL_720P)
			{
				gpio_i2c_write(vdec_slave_addr[i], 0xFF, 0x03); // clear sd/AHD status
				gpio_i2c_write(vdec_slave_addr[i], 0x0B, soft_reset[i]);
				gpio_i2c_write(vdec_slave_addr[i], 0x0B, 0x0);
				nvp6114_960H_setting(vdec_mode&0x01, 0x00, i);
				printk("IOC_VDEC_SET_VIDEO_MODE: set NVP6114_OUT_ALL_720P success\n");
			}
			else if(outmode[i] == NVP6114_OUT_ALL_960H)
			{
				gpio_i2c_write(vdec_slave_addr[i], 0xFF, 0x03); // clear sd/AHD status
				gpio_i2c_write(vdec_slave_addr[i], 0x0B, soft_reset[i]);
				gpio_i2c_write(vdec_slave_addr[i], 0x0B, 0x0);
				nvp6114_960H_setting(vdec_mode&0x01, 0x0F, i);
				printk("IOC_VDEC_SET_VIDEO_MODE: set NVP6114_OUT_ALL_960H success\n");
			}
			else if(outmode[i] == NVP6114_OUT_2X960_2X720P)
			{
				gpio_i2c_write(vdec_slave_addr[i], 0xFF, 0x03); // clear sd/AHD status
				gpio_i2c_write(vdec_slave_addr[i], 0x0B, soft_reset[i]);
				gpio_i2c_write(vdec_slave_addr[i], 0x0B, 0x0);
				nvp6114_960H_setting(vdec_mode&0x01, 0x03, i);
				//nvp6114_hybrid_mode_1(vdec_mode&0x01);
				printk("IOC_VDEC_SET_VIDEO_MODE: set NVP6114_OUT_2X960_2X720P success\n");
			}
			else if(outmode[i] == NVP6114_OUT_2X720P_2X960H)
			{
				gpio_i2c_write(vdec_slave_addr[i], 0xFF, 0x03); // clear sd/AHD status
				gpio_i2c_write(vdec_slave_addr[i], 0x0B, soft_reset[i]);
				gpio_i2c_write(vdec_slave_addr[i], 0x0B, 0x0);
				nvp6114_960H_setting(vdec_mode&0x01, 0x0C, i);
				//nvp6114_hybrid_mode_2(vdec_mode&0x01);
				printk("IOC_VDEC_SET_VIDEO_MODE: set NVP6114_OUT_2X720P_2X960H success\n");
			}
			else
			{
				printk("err!!! IOC_VDEC_SET_VIDEO_MODE not support outmode: %d\n", outmode[i]);
			}
		}
		mutex_unlock(&mutex);
//		nvp6114_init2();
//		enable_thread_flag = 1;
//		enable_thread_flag_cf = 1;
//		mutex_unlock(&mutex);
		break;
#if 1
		case IOC_VDEC_SET_BRIGHTNESS:
            		if(copy_from_user(&v_adj, argp, sizeof(vdec_video_adjust))) return -1;
			mutex_lock(&mutex);
			vdec_video_set_brightness(v_adj.ch, v_adj.value, vdec_mode&0x01);
			mutex_unlock(&mutex);
			break;
		case IOC_VDEC_SET_CONTRAST:
			if(copy_from_user(&v_adj, argp, sizeof(vdec_video_adjust))) return -1;
			mutex_lock(&mutex);
			vdec_video_set_contrast(v_adj.ch, v_adj.value, vdec_mode&0x01);
			mutex_unlock(&mutex);
			break;
		case IOC_VDEC_SET_HUE:
			if(copy_from_user(&v_adj, argp, sizeof(vdec_video_adjust))) return -1;
			mutex_lock(&mutex);
			vdec_video_set_hue(v_adj.ch, v_adj.value, vdec_mode&0x01);
			mutex_unlock(&mutex);
			break;
		case IOC_VDEC_SET_SATURATION:
			if(copy_from_user(&v_adj, argp, sizeof(vdec_video_adjust))) return -1;
			mutex_lock(&mutex);
			vdec_video_set_saturation(v_adj.ch, v_adj.value, vdec_mode&0x01);
			mutex_unlock(&mutex);
			break;
		case IOC_VDEC_SET_SHARPNESS:
			break;
#endif 
		case IOC_VDEC_PTZ_PELCO_INIT:
			mutex_lock(&mutex);
			vdec_coaxial_init();
			pelco_coax_mode();
			mutex_unlock(&mutex);
			break;
		case IOC_VDEC_PTZ_PELCO_RESET:
			mutex_lock(&mutex);
			pelco_reset();
			mutex_unlock(&mutex);
			break;
		case IOC_VDEC_PTZ_PELCO_SET:
			mutex_lock(&mutex);
			pelco_set();
			mutex_unlock(&mutex);
			break;
		case IOC_VDEC_PTZ_CHANNEL_SEL:
        	    	if (copy_from_user(&ptz_ch, argp, sizeof(ptz_ch)))
				return -1;			
			mutex_lock(&mutex);
			vdec_coaxial_select_ch(ptz_ch);
			mutex_unlock(&mutex);
			break;
		case IOC_VDEC_PTZ_PELCO_UP:
			mutex_lock(&mutex);
		 	pelco_up();
			mutex_unlock(&mutex);
		 	break;
		case IOC_VDEC_PTZ_PELCO_DOWN:
			mutex_lock(&mutex);
		 	pelco_down();
			mutex_unlock(&mutex);
		 	break;
		case IOC_VDEC_PTZ_PELCO_LEFT:
			mutex_lock(&mutex);
		 	pelco_left();
			mutex_unlock(&mutex);
		 	break;
		case IOC_VDEC_PTZ_PELCO_RIGHT:
			mutex_lock(&mutex);
			pelco_right();
			mutex_unlock(&mutex);
			break;
		case IOC_VDEC_PTZ_PELCO_FOCUS_NEAR:
			//FIXME
			mutex_lock(&mutex);
			pelco_osd();
			mutex_unlock(&mutex);
			//pelco_focus_near();
		 	break;
		case IOC_VDEC_PTZ_PELCO_FOCUS_FAR:
			//FIXME
			mutex_lock(&mutex);
			pelco_set();
			mutex_unlock(&mutex);
			//pelco_focus_far();
		 	break;
		case IOC_VDEC_PTZ_PELCO_ZOOM_WIDE:
			//pelco_zoom_wide();
			mutex_lock(&mutex);
			pelco_iris_open();
			mutex_unlock(&mutex);
		 	break;
		case IOC_VDEC_PTZ_PELCO_ZOOM_TELE:
			//pelco_zoom_tele();
			mutex_lock(&mutex);
			pelco_iris_close();
			mutex_unlock(&mutex);
			break;

		case IOC_VDEC_INIT_MOTION:
			mutex_lock(&mutex);
			vdec_motion_init();
			hi3520_init_blank_data(0);
			mutex_unlock(&mutex);
			break;
		case IOC_VDEC_ENABLE_MOTION:
			break;
		case IOC_VDEC_DISABLE_MOTION:
			break;
		case IOC_VDEC_SET_MOTION_AREA:
			break;
		case IOC_VDEC_GET_MOTION_INFO:
			mutex_lock(&mutex);
			vdec_get_motion_info(0);
			mutex_unlock(&mutex);
			break;
		case IOC_VDEC_SET_MOTION_DISPLAY:
            		if(copy_from_user(&on, argp, sizeof(unsigned int))) return -1;
			mutex_lock(&mutex);
			vdec_motion_display(0,on);
			mutex_unlock(&mutex);
			break;
		case IOC_VDEC_SET_MOTION_SENS:
            		if(copy_from_user(&sens, argp, sizeof(unsigned int)*16)) return -1;
			mutex_lock(&mutex);
			vdec_motion_sensitivity(sens);
			mutex_unlock(&mutex);
			break;
		case IOC_VDEC_ENABLE_LOW_RES:
			//vdec_low_resoultion_enable(0xff);
			break;
		case IOC_VDEC_DISABLE_LOW_RES:
			//vdec_low_resoultion_disable(0xff);
			break;
		case IOC_VDEC_ENABLE_BW:
			//vdec_bw_detection_enable(0xff);
			break;
		case IOC_VDEC_DISABLE_BW:
			//vdec_bw_detection_disable(0xff);
			break;
		case IOC_VDEC_READ_BALCK_COUNT:
			//value = vdec_bw_black_count_read(0);
			//copy_to_user(arg,&value, sizeof(int));
			break;
		case IOC_VDEC_READ_WHITE_COUNT:
			break;
		case IOC_VDEC_4CH_VIDEO_SEQUENCE:
			break;
#if 0
        case IOC_VIDEO_GET_VIDEO_MODE:
		case IOC_VIDEO_SET_MOTION:
        case IOC_VIDEO_GET_MOTION:
		case IOC_VIDEO_SET_MOTION_EN:
		case IOC_VIDEO_SET_MOTION_SENS:
		case IOC_VIDEO_SET_MOTION_TRACE:
        case IOC_VIDEO_GET_VIDEO_LOSS:
		case IOC_VIDEO_GET_IMAGE_ADJUST:
        case IOC_AUDIO_SET_SAMPLE_RATE:
        case IOC_AUDIO_SET_AUDIO_PLAYBACK:
        case IOC_AUDIO_SET_AUDIO_DA_VOLUME:
		case IOC_AUDIO_SET_BRIGHTNESS:
		case IOC_AUDIO_SET_CONTRAST:
		case IOC_AUDIO_SET_HUE:
		case IOC_AUDIO_SET_SATURATION:
		case IOC_AUDIO_SET_SHARPNESS:
		case IOC_AUDIO_SET_AUDIO_MUTE:
		case IOC_AUDIO_SET_LIVE_CH:

#endif
		case IOC_AUDIO_SET_PB_CH:
			if(copy_from_user(&audio_init_val, argp, sizeof(audio_init_val))) return -1;
			if(audio_init_val.sample == 8)
			{
				audio_init_val.sample = 0x00;
			}
			else
			{
				audio_init_val.sample = 0x01;
			}

			if(audio_init_val.bits== 16)
			{
				audio_init_val.bits = 0x00;
			}
			else
			{
				audio_init_val.bits = 0x01;
			}
			mutex_lock(&mutex);
			audio_init(vdec_slave_addr[0], audio_init_val.chn_cnt, audio_init_val.sample, audio_init_val.bits);
			mutex_unlock(&mutex);
			break;

		case IOC_AUDIO_SET_OUT_VOL:
			if(copy_from_user(&value, argp, sizeof(unsigned int))) return -1;
			if(13 > value)
				value = (value + 3)/4; //input: 0~15; 6114 valid: 0~3
			else
				value = 3; //input: 0~15; 6114 valid: 0~3
			mutex_lock(&mutex);
			set_audio_out_vol(vdec_slave_addr[0], value);
			mutex_unlock(&mutex);
			break;

		case IOC_AUDIO_SET_IN_VOL:
			if(copy_from_user(&value, argp, sizeof(unsigned int))) return -1;
			value *= 2; //input: 0~15; 6114 valid: 0~30
			mutex_lock(&mutex);
			set_audio_in_vol(vdec_slave_addr[0], value);
			mutex_unlock(&mutex);
			break;

		case IOC_AUDIO_SET_LIVE_CH:
		default:
            ////printk("drv:invalid nc decoder ioctl cmd[%x]\n", cmd);
			break;
	}
	return 0;
}

static struct file_operations vdec_fops = {
	.owner      = THIS_MODULE,
    //.ioctl      = vdec_ioctl,
    	.unlocked_ioctl	= vdec_ioctl,
	.open       = vdec_open,
	.read       = nvp6114_read,
	.poll       = nvp6114_poll,
	.release    = vdec_close
};

static struct miscdevice vdec_dev = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= "nvp6114dev",
	.fops  		= &vdec_fops,
};

static int __init vdec_module_init(void)
{
	int ret = 0, i = 0;

	for(i=0;i<4;i++)
	//for(i=0;i<4;i++)
	{
		chip_id[i] = check_id(vdec_slave_addr[i]);
		if( (chip_id[i] != NVP6114_R0_ID ) )
		{
			printk("nvp6114 Device ID Error... %x\n", chip_id[i]);
		}
		else
		{
			printk("nvp6114 Device (0x%x) ID OK... %x\n", vdec_slave_addr[i], chip_id[i]);
			vdec_cnt++;
		}
	}

	if(vdec_cnt <= 0)
	{
		printk("nvp6114 not exist ++++SSSS++++\n");
		return -1;
	}
	printk("NVP6114 Count = %x\n", vdec_cnt);

	ret = misc_register(&vdec_dev);
   	if (ret)
	{
		printk("ERROR: could not register NC Decoder devices:%#x \n",i);		
	}
	printk("NVP6114 Test Driver 2014.07.14\n");

#if 1	
	for(i=0;i<vdec_cnt;i++)
	{
		if(outmode[i] < 4)
		{
			//nvp6114_720p_30fps或nvp6114_720p_25fps这个是每个模式都要先设置的。
			if(outmode[i] == NVP6114_OUT_ALL_720P)
			{
				if(vdec_mode == VDEC_NTSC)
					nvp6114_720p_30fps(i, 0x0);
				else
					nvp6114_720p_25fps(i, 0x0);
				printk("NVP6114_OUT_ALL_720P\n");
			}
			else if(outmode[i] == NVP6114_OUT_ALL_960H)
			{
				if(vdec_mode == VDEC_NTSC)
					nvp6114_720p_30fps(i, 0x0f);
				else
					nvp6114_720p_25fps(i, 0x0f);
				nvp6114_960H_setting(vdec_mode&0x01, 0x0F, i);
				printk("NVP6114_OUT_ALL_960H\n");
			}
			else if(outmode[i] == NVP6114_OUT_2X960_2X720P)
			{
				if(vdec_mode == VDEC_NTSC)
					nvp6114_720p_30fps(i, 0x03);
				else
					nvp6114_720p_25fps(i, 0x03);
				nvp6114_960H_setting(vdec_mode&0x01, 0x03, i);
				//nvp6114_hybrid_mode_1(vdec_mode&0x01);
				printk("NVP6114_OUT_2X960_2X720P\n");
			}
			else if(outmode[i] == NVP6114_OUT_2X720P_2X960H)
			{
				if(vdec_mode == VDEC_NTSC)
					nvp6114_720p_30fps(i, 0x0c);
				else
					nvp6114_720p_25fps(i, 0x0c);
				nvp6114_960H_setting(vdec_mode&0x01, 0x0C, i);
				//nvp6114_hybrid_mode_2(vdec_mode&0x01);
				printk("NVP6114_OUT_2X720P_2X960H\n");
			}
			else
			{
				printk("err!!! init not support outmode: %d\n", outmode[i]);
			}
			gpio_i2c_write(vdec_slave_addr[i], 0xFF, 0x03);
			gpio_i2c_write(vdec_slave_addr[i], 0x0B, 0x0F);
			gpio_i2c_write(vdec_slave_addr[i], 0x0B, 0x0);
		}

	}
#endif
	memset(nvp6114_buf, 0x0,  sizeof(nvp6114_buf));
	memset(nvp6114_chn, 0x0,  sizeof(nvp6114_chn));
	mutex_init(&mutex);
	nvp6114_init2();
	enable_thread_flag = 0;
	enable_thread_flag_cf = 0;
	chg_flag = 0;
	nvp6114_task = kthread_run(nvp6114_thread, NULL, "knvp6114");
        if (IS_ERR( nvp6114_task))
        {
            PTR_ERR( nvp6114_task);
            nvp6114_task = NULL;
        }

        printk("init success\n");

	return 0;
}

static void __exit vdec_module_exit(void)
{
	misc_deregister(&vdec_dev);	
	if (nvp6114_task)
	{
		kthread_stop(nvp6114_task);
		nvp6114_task = NULL;
	}
}

module_init(vdec_module_init);
module_exit(vdec_module_exit);

MODULE_LICENSE("GPL");

