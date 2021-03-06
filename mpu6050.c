#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/cdev.h>


#define MPU6050_MAGIC 'K'

//mpu6050数据结构
union mpu6050_data
{
    struct {
        short x;
        short y;
        short z;
    }accel;
    struct {
        short x;
        short y;
        short z;
    }gyro;
    unsigned short temp;
};

//mpu6050的ioctl的命令定义
#define GET_ACCEL _IOR(MPU6050_MAGIC, 0, union mpu6050_data)//读取加速度计的数据
#define GET_GYRO  _IOR(MPU6050_MAGIC, 1, union mpu6050_data)//读取陀螺仪的数据 
#define GET_TEMP  _IOR(MPU6050_MAGIC, 2, union mpu6050_data)//读取温度的数据

#define SMPLRT_DIV      0x19    //陀螺仪采样率，典型值：0x07(125Hz)
#define CONFIG          0x1A    //低通滤波频率，典型值：0x06(5Hz)
#define GYRO_CONFIG     0x1B    //陀螺仪自检及测量范围，典型值：0x18(不自检，2000deg/s)
#define ACCEL_CONFIG    0x1C    //加速计自检、测量范围及高通滤波，典型值：0x18(不自检，2G，5Hz)
#define ACCEL_XOUT_H    0x3B
#define ACCEL_XOUT_L    0x3C
#define ACCEL_YOUT_H    0x3D
#define ACCEL_YOUT_L    0x3E
#define ACCEL_ZOUT_H    0x3F
#define ACCEL_ZOUT_L    0x40
#define TEMP_OUT_H      0x41
#define TEMP_OUT_L      0x42
#define GYRO_XOUT_H     0x43
#define GYRO_XOUT_L     0x44
#define GYRO_YOUT_H     0x45
#define GYRO_YOUT_L     0x46
#define GYRO_ZOUT_H     0x47    //陀螺仪z轴角速度数据寄存器（高位）
#define GYRO_ZOUT_L     0x48    //陀螺仪z轴角速度数据寄存器（低位）
#define PWR_MGMT_1      0x6B    //电源管理，典型值：0x00(正常启用)
#define WHO_AM_I        0x75    //IIC地址寄存器(默认数值0x68，只读)
#define SlaveAddress    0x68    //MPU6050-I2C地址寄存器


#define DEV_MINOR 100       //IIC从设备的起始次设备号
#define DEV_CNT 1           //IIC从设备的个数
#define DEV_NAME "mpu6050"  //IIC从设备名称

static struct i2c_client *mpu6050_client;

static struct cdev mpu6050_dev;
static dev_t mpu6050_devnum;       //设备号
static struct class *mpu6050_class;//设备类

/*
*    功能：向mpu6050从设备写入数据
*
*    参数：struct i2c_client *client：指向mpu6050从设备
*          const unsigned char reg：需写入的mpu6050的寄存器
*          const unsigned char val：写入的数值
*/
static void mpu6050_write_byte(struct i2c_client *client, const unsigned char reg, const unsigned char val)
{ 
    char txbuf[2] = {reg, val};//数据缓存buffer
    
	printk("mpu6050 write byte!\n");
    //封装msg
    struct i2c_msg msg[2] = {
    
        [0] = 
        {
            .addr = client->addr,
            .flags= 0,
            .len = sizeof(txbuf),
            .buf = txbuf,
        },
    };
    
    i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));//与从设备进行数据通信
}


/*
*    功能：向mpu6050从设备读取数据
*
*    参数：struct i2c_client *client：指向mpu6050从设备
*          const unsigned char reg：需读取的mpu6050的寄存器
*
*    返回值：char：读取的数据
*/
static char mpu6050_read_byte(struct i2c_client *client,const unsigned char reg)
{
    char txbuf[1] = {reg};//数据缓冲buffer
    char rxbuf[1] = {0};
    printk("mpu6050 read byte!\n");
    //封装msg
    struct i2c_msg msg[2] = 
    {
        [0] = 
        {
            .addr = client->addr,
            .flags = 0,
            .len = sizeof(txbuf),
            .buf = txbuf,
        },
        
        [1] = 
        {
            .addr = client->addr,
            .flags = I2C_M_RD,
            .len = sizeof(rxbuf),
            .buf = rxbuf,
        },
    };

    i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)); //与从设备进行数据通信
    
    return rxbuf[0];
}

//mpu6050硬件初始化
static void mpu6050_init(struct i2c_client *client)
{
    mpu6050_write_byte(client, PWR_MGMT_1, 0x00);
    mpu6050_write_byte(client, SMPLRT_DIV, 0x07);
    mpu6050_write_byte(client, CONFIG, 0x06);
    mpu6050_write_byte(client, GYRO_CONFIG, 0x18);
    mpu6050_write_byte(client, ACCEL_CONFIG, 0x0);
}


static int mpu6050_open(struct inode *ip, struct file *fp)
{
	printk("mpu6050 open success!\n");
    return 0;
}
static int mpu6050_release(struct inode *ip, struct file *fp)
{
	printk("mpu6050 release!\n");
    return 0;
}

static long mpu6050_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
    int res = 0;
    union mpu6050_data data = {{0}};
    printk("mpu6050 enter ioctl\n");
    switch(cmd)
    {
        //读取加速度计的数据
        case GET_ACCEL:
            data.accel.x = mpu6050_read_byte(mpu6050_client,ACCEL_XOUT_L);
            data.accel.x|= mpu6050_read_byte(mpu6050_client,ACCEL_XOUT_H)<<8;
            data.accel.y = mpu6050_read_byte(mpu6050_client,ACCEL_YOUT_L);
            data.accel.y|= mpu6050_read_byte(mpu6050_client,ACCEL_YOUT_H)<<8;
            data.accel.z = mpu6050_read_byte(mpu6050_client,ACCEL_ZOUT_L);
            data.accel.z|= mpu6050_read_byte(mpu6050_client,ACCEL_ZOUT_H)<<8;
            break;
        
        //读取陀螺仪的数据
        case GET_GYRO:
            data.gyro.x = mpu6050_read_byte(mpu6050_client,GYRO_XOUT_L);
            data.gyro.x|= mpu6050_read_byte(mpu6050_client,GYRO_XOUT_H)<<8;
            data.gyro.y = mpu6050_read_byte(mpu6050_client,GYRO_YOUT_L);
            data.gyro.y|= mpu6050_read_byte(mpu6050_client,GYRO_YOUT_H)<<8;
            data.gyro.z = mpu6050_read_byte(mpu6050_client,GYRO_ZOUT_L);
            data.gyro.z|= mpu6050_read_byte(mpu6050_client,GYRO_ZOUT_H)<<8;
            printk("gyro:x %d, y:%d, z:%d\n",data.gyro.x,data.gyro.y,data.gyro.z);
            break;
        
        //读取温度的数据
        case GET_TEMP:
            data.temp = mpu6050_read_byte(mpu6050_client,TEMP_OUT_L);
            data.temp|= mpu6050_read_byte(mpu6050_client,TEMP_OUT_H)<<8;
            printk("temp: %d\n",data.temp);
            break;
            
        default:
            printk(KERN_INFO "invalid cmd");
            break;
    }
    printk("acc:x %d, y:%d, z:%d\n",data.accel.x,data.accel.y,data.accel.z);
    res = copy_to_user((void *)arg,&data,sizeof(data));
    return sizeof(data);
}

//mpu6050操作集
static const struct file_operations mpu6050_fops = 
{
    .owner = THIS_MODULE,
    .open  = mpu6050_open,
    .release = mpu6050_release,
    .unlocked_ioctl = mpu6050_ioctl,
};

static int mpu6050_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct device *dev;
    
    mpu6050_client = client;
    
    printk("enter mpu6050_probe\n");
    /*****************************初始化硬件设备******************************/
    //初始化mpu6050
    mpu6050_init(client);
    printk("mpu6050 init success!\n");
    dbg("probe:name = %s,flag =%d,addr = %d,adapter = %d,driver = %s\n", client->name,
         client->flags,client->addr,client->adapter->nr,client->driver->driver.name );
    
    /*********************************创建接口********************************/
    cdev_init(&mpu6050_dev, &mpu6050_fops);                            //关联dev与fops
    alloc_chrdev_region(&mpu6050_devnum, DEV_MINOR, DEV_CNT, DEV_NAME);//自动分配设备号
    cdev_add(&mpu6050_dev, mpu6050_devnum, DEV_CNT);                   //添加设备至设备链表
    
    mpu6050_class = class_create(THIS_MODULE,DEV_NAME);                         //创建设备类
    dev = device_create(mpu6050_class, NULL , mpu6050_devnum, "%s%d", DEV_NAME);//创建mpu6050设备
    if (IS_ERR(dev))
    {
        dbg("device create error\n");
        goto out;
    }
    
    return 0;
out:
    return -1;
}

static int  mpu6050_remove(struct i2c_client *client)
{
    dbg("remove\n");
    printk("mpu6050 remove\n");
    device_destroy(mpu6050_class, mpu6050_devnum);
    class_destroy(mpu6050_class);
    unregister_chrdev_region(mpu6050_devnum,DEV_CNT);
    
    return 0;
}

//与mpu6050的设备信息匹配
static struct i2c_device_id mpu6050_ids[] = 
{
    {"mpu6050",0x68},
    {}
};

//声明mpu6050_ids是i2c类型的一个设备表
MODULE_DEVICE_TABLE(i2c,mpu6050_ids);

//定义并初始化从设备驱动信息
static struct i2c_driver mpu6050_driver = 
{
    .probe    = mpu6050_probe,
    .remove   = mpu6050_remove,
    .id_table = mpu6050_ids,
    .driver = 
    {
        .name = "mpu6050",
        .owner = THIS_MODULE,
    },
};

static int __init mpu6050_i2c_init(void)
{
    return i2c_add_driver(&mpu6050_driver);//注册设备驱动
}


static void __exit mpu6050_i2c_exit(void)
{
    i2c_del_driver(&mpu6050_driver);       //注销设备驱动
}

 
MODULE_AUTHOR("Lee");
MODULE_DESCRIPTION("mpu6050 driver");
MODULE_LICENSE("GPL");
module_init(mpu6050_i2c_init);
module_exit(mpu6050_i2c_exit);
