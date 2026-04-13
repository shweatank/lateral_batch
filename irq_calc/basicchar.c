#include<linux/module.h>
#include<linux/init.h>
#include<linux/kernel.h>
#include<linux/fs.h>     //register_chrdev,fileoperaations
#include<linux/uaccess.h>   //copy to iuser ,copy from user

#define DEVICE_NAME "basic char"
#define BUF_SIZE 256

static int major_number;
static char kernel_buffer[BUF_SIZE];
static int buffer_size;


/* 
called when user open /dev/basic_char
*/
static int basic_open(struct inode *inode,struct file *file)
{
    printk(KERN_INFO "baisc_char:device opened\n");
    return 0;
}
void reverse_buffer(char *buf, int len)
{
    int i = 0;
    int j = len - 1;
    char temp;

    while (i < j)
    {
        temp = buf[i];
        buf[i] = buf[j];
        buf[j] = temp;

        i++;
        j--;
    }
}

  void even(char *buf)
{
    int ret;
    int val;

    ret = kstrtoint(buf, 10, &val);
    if (ret < 0) {
        buffer_size = snprintf(buf, BUF_SIZE, "Invalid");
        return;
    }

    if (val % 2 == 0)
        buffer_size = snprintf(buf, BUF_SIZE, "Even");
    else
        buffer_size = snprintf(buf, BUF_SIZE, "Odd");
          printk(KERN_INFO"you have enterd a sucessful number\n");
}
      
void case_converter(char *str,int len)
{
  int i=0;
   while(i<len)
   {         
       if(str[i]>='a' && str[i]<='z')
        {
            str[i] = str[i] - 32; 
        }
        else if(str[i]>='A' && str[i]<='Z')
        {
            str[i] = str[i] + 32;
        }
        i++;
    }
 }
/*
*
*/
static int basic_release(struct inode *inode,struct file *file)
{
  printk(KERN_INFO "baisc_char:device  close\n");
    return 0;
}

static ssize_t basic_read(struct file *file,char __user *user_buffer,size_t count,loff_t *offset)
{
int bytes_to_copy;
/*
if offset is beyond data ,return 0(EOF)\

*/
  
  if(*offset >= buffer_size)
  return 0;
  bytes_to_copy=min(count,(size_t)(buffer_size - *offset));
  /*
   copy data from kernel space to user same 
  
   */  
   if(copy_to_user(user_buffer,kernel_buffer + *offset,bytes_to_copy))
   return -EFAULT;
   *offset += bytes_to_copy;
   printk(KERN_INFO"i am in read function \n");
   printk(KERN_INFO"basic char :read %d bytews\n",bytes_to_copy);
   return bytes_to_copy;
   }
   
 
   static ssize_t basic_write(struct file *file, const char __user *user_buffer,
                           size_t count, loff_t *offset)
{
    size_t bytes_to_copy;

    if (count == 0)
        return 0;

    bytes_to_copy = min(count, (size_t)(BUF_SIZE - 1));

    if (copy_from_user(kernel_buffer, user_buffer, bytes_to_copy))
        return -EFAULT;

    kernel_buffer[bytes_to_copy] = '\0';
    buffer_size=bytes_to_copy;
    if (bytes_to_copy > 0 && kernel_buffer[bytes_to_copy - 1] == '\n')
    {
        kernel_buffer[bytes_to_copy - 1] = '\0';
        buffer_size--;
    }
   
   // int num1,num2;
   //char op;
    even(kernel_buffer);                         
    reverse_buffer(kernel_buffer, buffer_size);
    case_converter(kernel_buffer, buffer_size);
    // sscanf(kernel_buffer, "%d %d %s", &num1, &num2,&op);
    /*
    switch (op)
    {
        case '+':
            result = num1 + num2;
            buffer_size = snprintf(kernel_buffer, BUF_SIZE, "Result = %d\n", result);
            break;

        case '-':
            result = num1 - num2;
            buffer_size = snprintf(kernel_buffer, BUF_SIZE, "Result = %d\n", result);
            break;

        case '*':
            result = num1 * num2;
            buffer_size = snprintf(kernel_buffer, BUF_SIZE, "Result = %d\n", result);
            break;

        case '/':
           
                result = num1 / num2;
                buffer_size = snprintf(kernel_buffer, BUF_SIZE, "Result = %d\n", result);
            
            break;

        default:
            buffer_size = snprintf(kernel_buffer, BUF_SIZE, "Invalid operator\n");
            break;
    }
    
    */
    printk(KERN_INFO "i am in write function\n");
    printk(KERN_INFO "basic_char :wrote %zu bytes\n", bytes_to_copy);

    return bytes_to_copy;
}
   
   /*file operataion structre
   this connect system calls to driver fnction
   */
   static struct file_operations basic_fops = {
    .owner= THIS_MODULE,
    .open = basic_open,
    .read = basic_read,
    .write = basic_write,
    .release = basic_release,
};

/*moduel initiliazation
*/
static int __init basic_char_init(void)
{
/*
register character device
0->dynamic major number
*/

major_number=register_chrdev(0,DEVICE_NAME,&basic_fops);
if(major_number<0)
{
printk(KERN_ERR "basic_char :failed to register device\n");
return major_number;
}
printk(KERN_INFO"basic_char loaded\n");
printk(KERN_INFO "Basic char major number =%d\n",major_number);
printk(KERN_INFO"create device node with \n");
printk(KERN_INFO"mknode /dev/%s c %d 0\n",DEVICE_NAME,major_number);
return 0;
}


static void __exit basic_char_exit(void)	
{
unregister_chrdev(major_number,DEVICE_NAME);
printk(KERN_INFO"basic_char:unloaded\n");
}

module_init(basic_char_init);
module_exit(basic_char_exit);

	

MODULE_LICENSE("GPL");
MODULE_AUTHOR("You");
MODULE_DESCRIPTION("Simple Character Driver");







 
   
   
   
   
