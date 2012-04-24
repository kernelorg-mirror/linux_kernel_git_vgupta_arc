/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Simon Spooner - Dec 2009 - Original version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>


#include <linux/in.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/init.h>
#include <linux/inetdevice.h>
#include <linux/inet.h>

#include <asm/cacheflush.h>
#include <asm/irq.h>
#include <linux/platform_device.h>
#include "xtemac.h"

#define DRIVER_NAME         "xilinx_temac"
#define TX_RING_LEN         2
#define RX_RING_LEN         32


/* Prototypes */

int xtemac_open(struct net_device *);
int xtemac_tx(struct sk_buff *, struct net_device *);
void xtemac_tx_timeout(struct net_device *);
void xtemac_set_multicast_list (struct net_device *);
void xtemac_tx_timeout(struct net_device *);
struct net_device_stats * xtemac_stats(struct net_device *);
int xtemac_stop(struct net_device *);
int xtemac_set_address(struct net_device *, void *);
void print_phy_status2(unsigned int );

static const struct net_device_ops xtemac_netdev_ops = {
	.ndo_open 			= xtemac_open,
	.ndo_stop 			= xtemac_stop,
	.ndo_start_xmit 	= xtemac_tx,
	.ndo_set_rx_mode = xtemac_set_multicast_list,
	.ndo_tx_timeout 	= xtemac_tx_timeout,
	.ndo_set_mac_address= xtemac_set_address,
    .ndo_get_stats      = xtemac_stats,
};


struct xtemac_stats
{
    unsigned int rx_packets;
    unsigned int rx_bytes;
};



struct xtemac_ptr
{
        struct xtemac_bridge bridge;
        struct xtemac_configuration config;
        struct xtemac_address_filter addrFilter;
        struct xtemac_bridge_extn bridgeExtn;

};

volatile struct xtemac_ptr *xtemac = (volatile struct xtemac_ptr *)XTEMAC_BASE;

volatile struct xtemac_priv
	{
		struct net_device_stats stats;
        spinlock_t      lock;
        unsigned char   address[6];
        unsigned int    RxEntries;
        unsigned int    RxCurrent;
        struct sk_buff  *tx_skb;
        struct net_device  *ndev;
        void *          tx_buff;
        void *          rx_buff;
        unsigned int    rx_len;
        struct  xtemac_stats phy_stats;

	};

volatile unsigned int debug=0;

void XEMAC_mdio_write (volatile struct xtemac_ptr *,unsigned int, unsigned int , unsigned int);
void xtemac_update_stats(struct xtemac_priv * ap);
unsigned int XEMAC_mdio_read ( volatile struct xtemac_ptr * , unsigned int, unsigned int, volatile unsigned int *);

extern struct sockaddr mac_addr;

/****************************/
/* XTEMAC interrupt handler */
/***************************/

static irqreturn_t
xtemac_emac_intr(int irq, void *dev_instance)
{

    volatile unsigned int    status;
    struct xtemac_priv  *priv = netdev_priv(dev_instance);
    struct sk_buff  *skb;

    status = arc_read_uncached_32(&xtemac->bridge.intrStsReg);


    if(status & INTR_STATUS_MDIO_BIT_POS)
    {
        if(debug)
            printk("TEMAC MDIO INT\n");

        arc_write_uncached_32(&xtemac->bridge.intrClrReg, INTR_STATUS_MDIO_BIT_POS);
    }


    if(status & INTR_STATUS_RX_ERROR_BIT_POS)
    {
        printk("TEMAC RX_ERROR\n");

        arc_write_uncached_32(&xtemac->bridge.intrClrReg, INTR_STATUS_RX_ERROR_BIT_POS);
        arc_write_uncached_32(&xtemac->bridgeExtn.intrEnableReg, ENABLE_ALL_INTR & (~ENABLE_RX_OVERRUN_INTR));
        printk("intrEnableReg = %x\n", arc_read_uncached_32(&xtemac->bridgeExtn.intrEnableReg));
    }

    if(status & INTR_STATUS_TX_ERROR_BIT_POS)
    {
        printk("TEMAC TX ERROR\n");
        arc_write_uncached_32(&xtemac->bridge.intrClrReg ,INTR_STATUS_TX_ERROR_BIT_POS);
    }

    if(status & INTR_STATUS_DMA_TX_ERROR_BIT_POS)
    {
        printk("TEMAC DMA TX ERROR\n");
        arc_write_uncached_32(&xtemac->bridge.intrClrReg, INTR_STATUS_DMA_TX_ERROR_BIT_POS);
    }
    if(status & INTR_STATUS_DMA_RX_ERROR_BIT_POS)
    {
        printk("TEMAC DMA RX ERROR\n");
        arc_write_uncached_32(&xtemac->bridge.intrClrReg, INTR_STATUS_DMA_RX_ERROR_BIT_POS);
    }

	/* RX FIFO DMA'd to main memory.
	 * -ack the interrupt
	 * -invalidate the cache line
	 * -setup an SKB and pass in the packet into the kernel.
	 * -re-enable RX interrupts to allow next packet in.
	 */
    if(status & INTR_STATUS_DMA_RX_COMPLETE_BIT_POS)
    {
        if(debug)
            printk("TEMAC DMA RX COMPLETE\n");
        arc_write_uncached_32(&xtemac->bridge.dmaRxClrReg, 1);
        arc_write_uncached_32(&xtemac->bridgeExtn.intrEnableReg,ENABLE_ALL_INTR & (~ENABLE_RX_OVERRUN_INTR));

        priv->stats.rx_packets++;
        priv->stats.rx_bytes += priv->rx_len;
        flush_and_inv_dcache_range(priv->rx_buff, priv->rx_buff +priv->rx_len);

        skb=alloc_skb(priv->rx_len + 32, GFP_ATOMIC);
        skb_reserve(skb,NET_IP_ALIGN);
        memcpy(skb->data, priv->rx_buff, priv->rx_len);
        skb_put(skb,priv->rx_len);
        skb->dev = dev_instance;
        skb->protocol = eth_type_trans(skb,dev_instance);
        skb->ip_summed = CHECKSUM_NONE;
        netif_rx(skb);

    }

	/* TX FIFO DMA'd out.
	 * ack the interrupt, reset the TX buffer
	 */
     if(status & INTR_STATUS_DMA_TX_COMPLETE_BIT_POS)
    {
        if(debug)
            printk("TEMAC DMA TX COMPLETE\n");
        arc_write_uncached_32(&xtemac->bridge.dmaTxClrReg , 1);
        priv->tx_skb = 0;
    }


	/* Receive a packet.
	 * Start to DMA into a physically contiguous buffer.
	 * Switch off RX interrupts until DMA complete.
	 */

    if(status & INTR_STATUS_RCV_INTR_BIT_POS )
    {
        if(debug)
            printk("TEMAC RCV INTR\n");
        arc_write_uncached_32(&xtemac->bridge.intrClrReg , INTR_STATUS_RCV_INTR_BIT_POS);
        arc_write_uncached_32(&xtemac->bridgeExtn.intrEnableReg , ENABLE_ALL_INTR & (~ENABLE_PKT_RECV_INTR ) & (~ENABLE_RX_OVERRUN_INTR) );

        priv->rx_len = arc_read_uncached_32(&xtemac->bridgeExtn.sizeReg);
        arc_write_uncached_32(&xtemac->bridge.dmaRxAddrReg , priv->rx_buff);
        inv_dcache_range(priv->rx_buff, priv->rx_buff + priv->rx_len);
        arc_write_uncached_32(&xtemac->bridge.dmaRxCmdReg , (priv->rx_len << 8 | DMA_READ_COMMAND));
    }


	/* Handle an RX FIFO overrun.
	 * ack the interrupt, handle this as a regular RX and DMA
	 * the packet into the RX buffer
	 */

    if(status & INTR_STATUS_RX_OVRN_INTR_BIT_POS)
    {
        printk("TEMAC RX OVERRUN\n");
        arc_write_uncached_32(&xtemac->bridge.dmaRxClrReg,1);
        arc_write_uncached_32(&xtemac->bridge.intrClrReg , INTR_STATUS_RX_OVRN_INTR_BIT_POS);
        arc_write_uncached_32(&xtemac->config.rxCtrl1Reg,0);
        arc_write_uncached_32(&xtemac->bridge.txFifoRxFifoRstReg, RESET_TX_RX_FIFO);
        arc_write_uncached_32(&xtemac->bridge.txFifoRxFifoRstReg, 0);

    }

	return IRQ_HANDLED;
}

int
xtemac_open(struct net_device * dev)
{

    unsigned int speed;
    unsigned int temp;

	/* OK, now switch on the hardware. */
    arc_write_uncached_32(&xtemac->config.rxCtrl1Reg,RESET_FIFO);
    arc_write_uncached_32(&xtemac->bridgeExtn.intrEnableReg,0);
    arc_write_uncached_32(&xtemac->bridge.intrClrReg , CLEAR_INTR_REG);
    arc_write_uncached_32(&xtemac->bridge.txFifoRxFifoRstReg, RESET_TX_RX_FIFO);
    arc_write_uncached_32(&xtemac->bridge.txFifoRxFifoRstReg,0);
    request_irq(dev->irq, xtemac_emac_intr, 0, dev->name, dev);
    arc_write_uncached_32(&xtemac->config.rxCtrl1Reg ,  ENABLE_RX );
    arc_write_uncached_32(&xtemac->config.txCtrlReg ,  ENABLE_TX);


    arc_write_uncached_32(&xtemac->config.macModeConfigReg, SET_100MBPS_MODE);


	/* Probe the Phy to see what speed is negotiated. */
     XEMAC_mdio_read(xtemac, BSP_XEMAC1_PHY_ID,MV_88E1111_STATUS2_REG, &speed);
    printk("Phy setup\n");
    print_phy_status2(speed);

    xtemac->addrFilter.addrFilterModeReg= ENABLE_ADDR_FILTER;

    xtemac_set_address(dev,&mac_addr);

     XEMAC_mdio_write(xtemac,BSP_XEMAC1_PHY_ID , MV_88E1111_CTRL_REG, MV_88E1111_CTRL_RESET);
     do {
        XEMAC_mdio_read(xtemac,BSP_XEMAC1_PHY_ID,MV_88E1111_CTRL_REG, &temp);
        } while(temp & MV_88E1111_CTRL_RESET);

    printk("XTEMAC - Transceiver reset\n");

    temp = MV_88E1111_AUTONEG_ADV_100BTX_FULL | MV_88E1111_AUTONEG_ADV_100BTX |                AUTONEG_ADV_IEEE_8023;

    XEMAC_mdio_write(xtemac, BSP_XEMAC1_PHY_ID, MV_88E1111_AUTONEG_ADV_REG, temp);

    printk("XTEMAC - Advertise capabilities\n");

	/* Autonegotiate the connection */
    XEMAC_mdio_write(xtemac,BSP_XEMAC1_PHY_ID,MV_88E1111_CTRL_REG, (MV_88E1111_CTRL_AUTONEG | MV_88E1111_CTRL_RESTART_AUTO));

    do {
        XEMAC_mdio_read(xtemac,BSP_XEMAC1_PHY_ID,MV_88E1111_STATUS_REG, &temp);
        } while( !(temp & MV_88E1111_STATUS_COMPLETE));
    printk("XTEMAC - Autonegotiate complete\n");

   XEMAC_mdio_read(xtemac,BSP_XEMAC1_PHY_ID,MV_88E1111_STATUS2_REG, &temp);

   if (temp & MV_88E1111_STATUS2_FULL)
        printk("XTEMAC - Full Duplex\n");
    else
        printk("XTEMAC - Not Full Duplex\n");

	/* Go ! */

    arc_write_uncached_32(&xtemac->bridgeExtn.intrEnableReg , ENABLE_ALL_INTR & (~ENABLE_RX_OVERRUN_INTR));

return 0;
}

/* XTEMAC close routine */
int
xtemac_stop(struct net_device * dev)
{
	return 0;
}

/* XTEMAC ioctl commands */
int
xtemac_ioctl(struct net_device * dev, struct ifreq * rq, int cmd)
{
	printk("ioctl called\n");
	/* FIXME :: not ioctls yet :( */
	return (-EOPNOTSUPP);
}

/* XTEMAC transmit routine */


int
xtemac_tx(struct sk_buff * skb, struct net_device * dev)
{
    struct xtemac_priv *priv = netdev_priv(dev);
    unsigned int tx_space;

    if(priv->tx_skb)
    {
/*        printk("Dropping due to previous TX not complete\n"); */
        return NETDEV_TX_BUSY;
    }

    tx_space = arc_read_uncached_32(&xtemac->bridge.macTxFIFOStatusReg);
    if(tx_space < skb->len)
    {
/*        printk("Dropping due to not enough room in the TXFIFO\n"); */
        return NETDEV_TX_BUSY;
    }

    memcpy(priv->tx_buff, skb->data, skb->len);
    flush_and_inv_dcache_range(priv->tx_buff, priv->tx_buff + skb->len);
    arc_write_uncached_32(&xtemac->bridge.dmaTxAddrReg , priv->tx_buff);
    arc_write_uncached_32(&xtemac->bridge.dmaTxCmdReg , (skb->len <<8 | DMA_WRITE_COMMAND | DMA_START_OF_PACKET | DMA_END_OF_PACKET));
    priv->stats.tx_packets++;
    priv->stats.tx_bytes += skb->len;

    priv->tx_skb = skb;
    dev_kfree_skb(priv->tx_skb);
    return(0);

}


/* the transmission timeout function */
void
xtemac_tx_timeout(struct net_device * dev)
{
	printk("transmission timed out\n");
	printk(KERN_CRIT "transmission timeout\n");
	return;
}

/* the set multicast list method */
void
xtemac_set_multicast_list(struct net_device * dev)
{
	printk("set multicast list called\n");
	return;
}

int
xtemac_set_address(struct net_device * dev, void *p)
{

    int             i;
    struct sockaddr *addr = p;
    unsigned int temp;

    memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);
    printk(KERN_INFO "MAC address set to ");
    for (i = 0; i < 6; i++)
        printk("%02x:", dev->dev_addr[i]);
    printk("\n");

    temp = dev->dev_addr[3];
    temp = ((temp << 8) | (dev->dev_addr[2]));
    temp = ((temp << 8) | (dev->dev_addr[1]));
    temp = ((temp << 8) | (dev->dev_addr[0]));
    arc_write_uncached_32(&xtemac->addrFilter.unicastAddr0Reg ,temp);

    temp = dev->dev_addr[5];
    temp = ((temp << 8) | (dev->dev_addr[4]));

    arc_write_uncached_32(&xtemac->addrFilter.unicastAddr1Reg, temp);

	return 0;
}

void xtemac_update_stats( struct xtemac_priv *ap)
{
}

struct net_device_stats *
xtemac_stats(struct net_device * dev)
{

    unsigned long   flags;
    struct xtemac_priv *ap = netdev_priv(dev);

    spin_lock_irqsave(&ap->lock, flags);
    xtemac_update_stats(ap);
    spin_unlock_irqrestore(&ap->lock, flags);

    return (&ap->stats);



}


static int __devinit xtemac_probe(struct platform_device *dev)
{

    struct net_device *ndev;
    struct xtemac_priv *priv;
    unsigned int rc;

printk("Probing...\n");

    ndev = alloc_etherdev(sizeof(struct xtemac_priv));
    if (!ndev)
    {
        printk("Failed to allocated net device\n");
        return -ENOMEM;
    }

    SET_NETDEV_DEV(ndev, &dev->dev);
	platform_set_drvdata(dev, ndev);

    ndev->irq = 6;
    xtemac_set_address(ndev,&mac_addr);
    priv = netdev_priv(ndev);
    priv->ndev = ndev;
    priv->tx_skb = 0;

    ndev->netdev_ops = &xtemac_netdev_ops;
    ndev->watchdog_timeo = (400*HZ/1000);
    ndev->flags &= ~IFF_MULTICAST;

    priv->rx_buff = kmalloc(4096,GFP_ATOMIC | GFP_DMA);
    priv->tx_buff = kmalloc(4096,GFP_ATOMIC | GFP_DMA);

    printk("RX Buffer @ %x , TX Buffer @ %x\n", (unsigned int)priv->rx_buff,(unsigned int) priv->tx_buff);

    if(!priv->rx_buff || !priv->tx_buff)
    {
        printk("Failed to alloc FIFO buffers\n");
        return -ENOMEM;
    }

    spin_lock_init(&((struct xtemac_priv *) netdev_priv(ndev))->lock);

    rc = register_netdev(ndev);
    if (rc)
    {
        printk("Didn't register the netdev.\n");
        return(rc);
    }

   return(0);

}

static void xtemac_remove(const struct net_device *dev)
{
    struct net_device *ndev = dev_get_drvdata(dev);
    unregister_netdev(ndev);

}

void print_phy_status2(unsigned int status)
{

    printk("Status is %x\n", status);

    if(status & MV_88E1111_STATUS2_FULL)
        printk("Full duplex\n");
    if(status & MV_88E1111_STATUS2_LINK_UP)
        printk("Link up\n");
    if(status & MV_88E1111_STATUS2_100)
        printk("100Mbps\n");
    if(status & MV_88E1111_STATUS2_1000)
        printk("1000Mbps\n");
}

void  XEMAC_mdio_write
    (
      /* [IN] the device physical address */
      volatile struct xtemac_ptr *dev_ptr,

      /* [IN] The device physical address */
      unsigned int              phy_addr,

      /* [IN] the register index (0-31)*/
      unsigned int              reg_index,

      /* [IN] The data to be written to register */
      unsigned int              data
   )
{ /* Body */
    unsigned int          value;

    arc_write_uncached_32(&dev_ptr->config.mgmtConfigReg, MDIO_ENABLE);
    arc_write_uncached_32(&dev_ptr->bridgeExtn.phyAddrReg, phy_addr);
    arc_write_uncached_32((&dev_ptr->bridgeExtn.macMDIOReg + reg_index), data);

    value = arc_read_uncached_32(&dev_ptr->bridgeExtn.intrRawStsReg);

    while (!(value & INTR_STATUS_MDIO_BIT_POS))
    {
        value = arc_read_uncached_32(&dev_ptr->bridgeExtn.intrRawStsReg);
    }

    arc_write_uncached_32(&dev_ptr->bridge.intrClrReg, INTR_STATUS_MDIO_BIT_POS);

} /* Endbody */




/* [IN] The device physical address */
/* [IN] the register index (0-31)*/
/* [IN]/[OUT] The data pointer */
unsigned int XEMAC_mdio_read ( volatile struct xtemac_ptr * dev_ptr,
     unsigned int                phy_addr,
     unsigned int                reg_index,
     volatile unsigned int                * data_ptr
  )
{
    unsigned int value = 0;
    unsigned int count=0;

    arc_write_uncached_32(&dev_ptr->config.mgmtConfigReg , MDIO_ENABLE);
    arc_write_uncached_32(&dev_ptr->bridgeExtn.phyAddrReg , phy_addr);
   *data_ptr = arc_read_uncached_32(&dev_ptr->bridgeExtn.macMDIOReg + reg_index);

   value = arc_read_uncached_32(&dev_ptr->bridgeExtn.intrRawStsReg);

   while (!(value & INTR_STATUS_MDIO_BIT_POS))
   {
       count++;
       value = arc_read_uncached_32(&dev_ptr->bridgeExtn.intrRawStsReg);
   }

   arc_write_uncached_32(&dev_ptr->bridge.intrClrReg ,  INTR_STATUS_MDIO_BIT_POS);
    return 0;
}

static struct platform_driver xtemac_driver = {
     .driver = {
         .name = DRIVER_NAME,
     },
     .probe = xtemac_probe,
     .remove = xtemac_remove
};



int __init
xtemac_module_init(void)
{

printk("*** Init XTEMAC ***\n");
    return platform_driver_register(&xtemac_driver);


}

void __exit
xtemac_module_cleanup(void)
{
	return;

}


module_init(xtemac_module_init);
module_exit(xtemac_module_cleanup);

static __init add_xtemac(void)
{
    struct platform_device *pd;
    pd = platform_device_register_simple("xilinx_temac",0,NULL,0);

    if (IS_ERR(pd))
    {
        printk("Fail\n");
    }

    return IS_ERR(pd) ? PTR_ERR(pd): 0;
}

device_initcall(add_xtemac);
