#include <linux/kernel.h>
#include <linux/mailbox_client.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <asm/io.h>
#include <linux/mfd/ma35d1-sys.h>

#define RTP_KWS_DEV_NAME	"rtp-kws"
#define NUM_PRESSED 	1
#define NUM_RELEASED 	0

#define INFO_BLOCK_SIZE	16
#define NUMS_KEYS	16
#define NUMS_SLIDER	2
#define NUMS_WHEEL	2

#define KEYS_IDX		0	
#define SLIDER0_IDX		12
#define SLIDER1_IDX		13
#define WHEEL0_IDX		14
#define WHEEL1_IDX		15

#define SLIDER0_TRACKING_ID		0
#define SLIDER1_TRACKING_ID		1
#define WHEEL0_TRACKING_ID		2
#define WHEEL1_TRACKING_ID		3


static void __iomem* RES_SY_IPRST0;
static void __iomem* RES_SYS_RLKTZNS;
static void __iomem* RES_SYS_CHIPCFG;
static void __iomem* RES_SSPCC_SINFAEN;
static void __iomem* RES_DPM_M4DS;
static void __iomem* RES_DPM_M4NDS;
static void __iomem* RES_PLM_STS;


struct rtp_kws {
	struct device *dev;
	struct mbox_chan *ch;
	struct input_dev *input;

	u8 num_keycodes;
	u32 keycodes[NUMS_KEYS];

};

static void rtp_kws_get_remote_msg(struct mbox_client *cli, void *msg)
{
	struct rtp_kws *dev = dev_get_drvdata(cli->dev);
	u32 *data = (u32 *)msg;
	u32 status[4];
	int i;

	u16 num_spot;
	
	if(!msg){
		printk("msg is empty!\n");
		return;
	}
			
	if (num_spot < 0 || num_spot > 15){
		printk("Out of Range!");
		return;
	} 
	

	for (i = 0; i < 4; i++)
		status[i] = *data++;
	
	num_spot = (u16)status[0];
	//printk("Report keycode: %d \n", dev->keycodes[num_spot]);
	input_report_key(dev->input, dev->keycodes[num_spot], NUM_PRESSED);
	input_sync(dev->input);
	
	int t_delay = 1000;
	while(--t_delay){}
	
	input_report_key(dev->input, dev->keycodes[num_spot], NUM_RELEASED);
	input_sync(dev->input);
	
}

static struct mbox_chan * 
rtp_kws_request_channel(struct platform_device *pdev)
{
	struct mbox_client *client;
	struct mbox_chan *ch;

	client = devm_kzalloc(&pdev->dev, sizeof(*client), GFP_KERNEL);
	if (!client) 
		return ERR_PTR(-ENOMEM);

	client->dev = &pdev->dev;
	client->rx_callback = rtp_kws_get_remote_msg;
	client->tx_done = NULL;
	client->tx_block = false;
	client->knows_txdone = false;
	client->tx_tout = 100;

	ch = mbox_request_channel(client, 0);
	if (IS_ERR(ch)) {
		return NULL;
	}

	return ch;
}

static int rtp_kws_probe(struct platform_device *pdev)
{
	struct rtp_kws *tdev;
	struct input_dev *input;
	struct device_node *np = pdev->dev.of_node;
	int err;
	int i;

	tdev = devm_kzalloc(&pdev->dev, sizeof(*tdev), GFP_KERNEL);
	if (!tdev)
		return ERR_PTR(-ENOMEM);

	tdev->ch = rtp_kws_request_channel(pdev);
	if (!tdev->ch)
		return -EPROBE_DEFER;

	/* input device */
	input = devm_input_allocate_device(&pdev->dev);
	if (!input) {
		return -ENOMEM;
	}

	input->name = RTP_KWS_DEV_NAME;
	input->id.bustype = BUS_HOST;
	
	tdev->input = input;
	input_set_drvdata(input, tdev);

	/* Keys */
	tdev->num_keycodes = NUMS_KEYS;

	/* default keycodes */
	tdev->keycodes[0] = KEY_0;
	tdev->keycodes[1] = KEY_1;
	tdev->keycodes[2] = KEY_2;
	tdev->keycodes[3] = KEY_3;
	tdev->keycodes[4] = KEY_4;
	tdev->keycodes[5] = KEY_5;
	tdev->keycodes[6] = KEY_6;
	tdev->keycodes[7] = KEY_7;
	tdev->keycodes[8] = KEY_8;
	tdev->keycodes[9] = KEY_9;
	tdev->keycodes[10] = KEY_F1;
	tdev->keycodes[11] = KEY_F2;
	tdev->keycodes[12] = KEY_F3;
	tdev->keycodes[13] = KEY_F4;
	tdev->keycodes[14] = KEY_F5;
	tdev->keycodes[15] = KEY_F6;

	

	for (i = 0; i < tdev->num_keycodes; i++)
		input_set_capability(input, EV_KEY, tdev->keycodes[i]);

	err = input_register_device(input);
	if (err) {
		dev_err(&pdev->dev,
			"failed to register input device: %d\n", err);
		return err;
	}

	tdev->dev = &pdev->dev;
	platform_set_drvdata(pdev, tdev);

	dev_info(&pdev->dev, "Successfully registered\n");

	return 0;
}

static int rtp_kws_remove(struct platform_device *pdev)
{
	iounmap(RES_SYS_CHIPCFG);
	iounmap(RES_SSPCC_SINFAEN);
	iounmap(RES_DPM_M4DS);
	iounmap(RES_DPM_M4NDS);
	iounmap(RES_PLM_STS);
	
	struct rtp_kws *tdev = platform_get_drvdata(pdev);
	
	if (tdev->ch) {
		mbox_free_channel(tdev->ch);
	}

	input_unregister_device(tdev->input);
	input_free_device(tdev->input);

	return 0;
}

static const struct of_device_id rtp_kws_of_match[] = {
	{ .compatible = "nuvoton,rtp-kws", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, rtp_kws_of_match);

static struct platform_driver rtp_kws_driver = {
	.driver = {
		.name	= RTP_KWS_DEV_NAME,
		.of_match_table = of_match_ptr(rtp_kws_of_match),
	},
	.probe		= rtp_kws_probe,
	.remove		= rtp_kws_remove,
};
module_platform_driver(rtp_kws_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Nuvoton RTP Key Word Spot Driver");
MODULE_AUTHOR("Nuvoton, Owen Huang");
