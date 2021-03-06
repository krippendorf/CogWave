/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
///////                            Author: Vincent Le Nir                                         ///////
///////                                 Royal Military Academy                                    ///////
///////           Department Communications, Information Systems & Sensors (CISS)                 ///////
///////                             30, Avenue de la Renaissance                                  ///////
///////                                B-1000 Brussels BELGIUM                                    ///////
///////                                   Tel: +3227426624                                        ///////
///////                                 email:vincent.lenir@rma.ac.be                             ///////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "tdma_tdd_performance_test.h"
#include <net/if.h>
#include <linux/if_tun.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

TDMA_TDD_Performance_Test::TDMA_TDD_Performance_Test(Ui_MainWindow *ui)
{

    number_transceivers=4;
    ivec fd_ext;
    fd_ext.set_size(number_transceivers);
    //create transceivers
    for(int i=0;i<number_transceivers;i++){
        stringstream tun_i;
        tun_i << "tun" << i;
        fd_ext[i]=tun_alloc((char*)(tun_i.str().c_str()));
        stringstream cmd;
        cmd << "ifconfig " <<  tun_i.str() << " 192.168.1." << i+1 << " netmask 255.255.255.0 broadcast 192.168.1.255";
        popen(cmd.str().c_str(),"r");
        TDMA_TDD_TX *tmp=new TDMA_TDD_TX(ui);
        TDMA_TDD_RX *tmp2=new TDMA_TDD_RX(ui);
        transmitters.push_back(tmp);
        receivers.push_back(tmp2);
    }

    //Connect transmitters with their corresponding receivers
    for(int i=0;i<number_transceivers;i++){

        QObject::connect(transmitters[i],SIGNAL(valuechanged(bool)),receivers[i],SLOT(setvalue(bool)),Qt::BlockingQueuedConnection);
        QObject::connect(receivers[i],SIGNAL(valuechanged(bool,double)),transmitters[i],SLOT(setvalue(bool,double)),Qt::BlockingQueuedConnection);
        QObject::connect(receivers[i],SIGNAL(valuechanged(int)),transmitters[i],SLOT(setvalue(int)),Qt::BlockingQueuedConnection);
        QObject::connect(receivers[i],SIGNAL(waveformset(int)),transmitters[i],SLOT(setwaveform(int)),Qt::BlockingQueuedConnection);
        QObject::connect(receivers[i],SIGNAL(slot_allocation(int)),transmitters[i],SLOT(slot_allocated(int)),Qt::BlockingQueuedConnection);

    }
    //connect to simulator device
    qRegisterMetaType<timestamp_data>("timestamp_data");
    for(int i=0;i<number_transceivers;i++){
        for(int j=0;j<number_transceivers;j++){
            QObject::connect(transmitters[i]->device,SIGNAL(send_vector(timestamp_data)),receivers[j]->device,SLOT(recv_vector(timestamp_data)),Qt::BlockingQueuedConnection);
        }
        QObject::connect(receivers[i]->device,SIGNAL(updated_timer(double)),transmitters[i]->device,SLOT(update_timer(double)),Qt::BlockingQueuedConnection);
    }

    //Fix positions of the nodes and calculate distance matrix
    cvec positions(number_transceivers);
    for (int i=0;i<number_transceivers;i++){
        positions[i]=complex<double>(0,0)+0.010*i*complex<double>(1,1);
    }
    for (int i=0;i<number_transceivers;i++){
        receivers[i]->device->channel_model->calculate_distance_matrix(positions);
    }

    //Determine which transceivers talk to which transceivers
    for(int i=0;i<number_transceivers;i++){
        transmitters[i]->myaddress=i;
        receivers[i]->myaddress=i;
        transmitters[i]->device->my_address=i;
        receivers[i]->device->my_address=i;
        if(number_transceivers==1){
            transmitters[i]->destaddress=i;
            receivers[i]->destaddress=i;
        }
        else{
            if((i/2)*2==i){
                transmitters[i]->destaddress=i+1;
                receivers[i]->destaddress=i+1;
            }
            else{
                transmitters[i]->destaddress=i-1;
                receivers[i]->destaddress=i-1;
            }
        }
    }
    //start transceivers
    for(int i=0;i<number_transceivers;i++){
        receivers[i]->start();
        transmitters[i]->start();
    }

    sleep(1);
    //Start BER test TX and RX
    for(int i=0;i<number_transceivers;i++){
        transmitters[i]->packet->ptr=fd_ext[i];
        receivers[i]->packet->ptr=fd_ext[i];
        transmitters[i]->is_time_set=true;
        transmitters[i]->tx_timestamp=i*transmitters[i]->time_gap/transmitters[i]->tdma_slots;
        transmitters[i]->packet->is_ip=true;
        receivers[i]->packet->is_ip=true;
        transmitters[i]->state="SEND";
    }
    //transmitters[1]->packet->is_ber_count=false;
    //transmitters[1]->state="NOSTATE";

}

int TDMA_TDD_Performance_Test::tun_alloc(char *dev)
{
    struct ifreq ifr;
    int fd, err;

    if( (fd = open("/dev/net/tun", O_RDWR)) < 0 )
       return -1;

    memset(&ifr, 0, sizeof(ifr));

    /* Flags: IFF_TUN   - TUN device (no Ethernet headers)
     *        IFF_TAP   - TAP device
     *
     *        IFF_NO_PI - Do not provide packet information
     */
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    if( *dev )
       strncpy(ifr.ifr_name, dev, IFNAMSIZ);

    if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ){
       ::close(fd);
       return err;
    }
    return fd;
}


void TDMA_TDD_Performance_Test::run(){

    int nb_thread_sync=0;
    while(1){
       nb_thread_sync=0;
       for(int i=0;i<number_transceivers;i++){
            if(receivers[i]->device->is_synchronized==false)
                    nb_thread_sync++;
       }
       if(nb_thread_sync==number_transceivers){
           for(int i=0;i<number_transceivers;i++){
                receivers[i]->device->is_synchronized=true;
           }
       }
    }

}

