#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/io.h>

MODULE_AUTHOR("Hideaki Komiya");
MODULE_DESCRIPTION("driver for LED control");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.0.1");

static dev_t dev;
static struct cdev cdv; //キャラクタデバイスの構造体を作る
static struct class *cls = NULL;
static volatile u32 *gpio_base = NULL;

//デバイスファイルに書き込みがあった時の挙動
static ssize_t led_write(struct file* filp, const char* buf, size_t count, loff_t* pos)
{
	char c;
	if(copy_from_user(&c,buf,sizeof(char)))
	return -EFAULT;

	if(c == '0')
		gpio_base[10] = 1 << 25;
	else if(c == '1')
		gpio_base[7] = 1 << 25;
	return 1; //読み込んだ文字数を返す（この場合はダミーの1)
}

//デバイスファイルから出力
static ssize_t sushi_read(struct file* filp, char* buf, size_t count, loff_t* pos)
{
	int size = 0;
	char sushi[] = {'s','u','s','h','i',0x0A};
	if(copy_to_user(buf+size,(const char *)sushi, sizeof(sushi))){
		printk( KERN_INFO "sushi : copy_to_user failed\n" );
		return -EFAULT;
	}
	size += sizeof(sushi);
	return size;
}

//挙動を書いた関数のポインタを格納する構造体(VFSに伝える)
static struct file_operations led_fops = {
	.owner = THIS_MODULE,//read,writeの定義をここに記載（重要）
	.write = led_write,
	.read = sushi_read
};


static int __init init_mod(void)
{
	//デバイス番号を取得する部分
	int retval;

	//ラズパイのレジスタ制御準備
	gpio_base = ioremap(0x3f200000, 0xA0); //0xfe..:base address, 0xA0: region to map
	const u32 led = 25;
	const u32 index = led/10;//GPFSEL2
	const u32 shift = (led%10)*3;//15bit
	const u32 mask = ~(0x7 << shift);//11111111111111000111111111111111
	gpio_base[index] = (gpio_base[index] & mask) | (0x1 << shift);//001: output flag
	//11111111111111001111111111111111

	retval = alloc_chrdev_region(&dev,0,1,"myled"); //0番からマイナ番号を取得
	if(retval < 0){
		printk(KERN_ERR "alloc_chrdev_region failed\n");
		return retval;
	}

	printk(KERN_INFO "%s is loaded major:%d\n",__FILE__,MAJOR(dev));

	//キャラクタデバイスの初期化
	cdev_init(&cdv,&led_fops);
	retval = cdev_add(&cdv,dev,1);
	if(retval < 0){
		printk(KERN_ERR "cdev_add failed. major:%d,minor:%d\n",MAJOR(dev),MINOR(dev));
		return retval;
	}

	//クラスを作成する
	cls = class_create(THIS_MODULE,"myled"); //THIS_MODULE:このモジュールを管理する構造体のポインタ
	if(IS_ERR(cls)){
		printk(KERN_ERR "class_dreate failed.");
		return PTR_ERR(cls);
	}
	
	//デバイス情報の作成
	device_create(cls,NULL,dev,NULL,"myled%d",MINOR(dev));

	return 0;
}

static void __exit cleanup_mod(void)
{
	cdev_del(&cdv);
	device_destroy(cls,dev);
	class_destroy(cls);
	unregister_chrdev_region(dev,1);
	printk(KERN_INFO "%s is unloaded. major:%d\n",__FILE__,MAJOR(dev));
	iounmap(gpio_base);
}

module_init(init_mod);
module_exit(cleanup_mod);
