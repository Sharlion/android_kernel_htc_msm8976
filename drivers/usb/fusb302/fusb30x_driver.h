
#ifndef FUSB30X_DRIVER_H
#define	FUSB30X_DRIVER_H

#ifdef	__cplusplus
extern "C" {
#endif

#define FUSB30X_I2C_DRIVER_NAME                 "fusb302"                                                       
#define FUSB30X_I2C_DEVICETREE_NAME             "fairchild,fusb302"                                             
#define FUSB30X_I2C_SMBUS_BLOCK_REQUIRED_FUNC   (I2C_FUNC_SMBUS_I2C_BLOCK)                                      
#define FUSB30X_I2C_SMBUS_REQUIRED_FUNC         (I2C_FUNC_SMBUS_WRITE_I2C_BLOCK | \
                                                I2C_FUNC_SMBUS_READ_BYTE_DATA)                                  

#ifdef CONFIG_OF                                                                
static const struct of_device_id fusb30x_dt_match[] = {                         
    { .compatible = FUSB30X_I2C_DEVICETREE_NAME },                              
    {},
};
MODULE_DEVICE_TABLE(of, fusb30x_dt_match);
#endif	

static const struct i2c_device_id fusb30x_i2c_device_id[] = {
    { FUSB30X_I2C_DRIVER_NAME, 0 },
    {}
};
MODULE_DEVICE_TABLE(i2c, fusb30x_i2c_device_id);                                

static int fusb30x_pm_suspend(struct device *dev);
static int fusb30x_pm_resume(struct device *dev);

static int __init fusb30x_init(void);                                                                   
static void __exit fusb30x_exit(void);                                                                  
static int fusb30x_probe(struct i2c_client* client,                                                     
                         const struct i2c_device_id* id);
static int fusb30x_remove(struct i2c_client* client);                                                   

static const struct dev_pm_ops fusb30x_pm_ops = {
	.suspend = fusb30x_pm_suspend,
	.resume = fusb30x_pm_resume,
};


static struct i2c_driver fusb30x_driver = {
    .driver = {
        .name = FUSB30X_I2C_DRIVER_NAME,                                        
		.pm = &fusb30x_pm_ops,
        .of_match_table = of_match_ptr(fusb30x_dt_match),                       
    },
    .probe = fusb30x_probe,                                                     
    .remove = fusb30x_remove,                                                   
    .id_table = fusb30x_i2c_device_id,                                          
};

#ifdef	__cplusplus
}
#endif

#endif	

