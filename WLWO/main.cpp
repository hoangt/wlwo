/*****************************************************
This file includes entrance "main" and the framwork of
the whole work.
******************************************************/
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstdio>
#include <cstring>
#include "space.h"
#include "global.h"
#include "startgap.h"
#include "securityrefresh.h"
using namespace std;


ofstream outfile(result_path,ofstream::out|ofstream::app);
#ifdef DEBUG
unsigned int a_access_count=0;
#endif // DEBUG


/** \brief access_line: This function is the main framwork of the whole work.
 *
 * \param  line_address: The address of a line from the last level cache to the memory.
 * \param  update: access happens when perfrom security refreshing
 * \param  deepth: the deepth of pointer which points to line_address
 * \return bool: A return "true" indicates the success of the access.
 *
 */
bool access_line(unsigned int line_address,bool update,int deepth)//update:whether to update pointer deepth
{

    //we do not need to consider read access.
#ifdef DEBUG
    if(line_address==13421788)
    {
        a_access_count++;
    }
#endif
    if((strcmp(wl_method,"start_gap")==0))
    {
        line_address=random_map[line_address];
    }
    unsigned int mapped_address = wear_leveling_map(line_address,wl_method,false);
#ifdef DEBUG
    if(deepth==10000-20)
    {
        int i=0;
        i++;
    }
    if((line_address>0)&&(line_address==pcm.lines[mapped_address].remap_address))
    {
        cout<<"line address = "<<line_address<<endl;
    }
#endif
    if(update)
    {
        pcm.lines[mapped_address].point_deep=deepth+1;
    }
    if(pcm.lines[mapped_address].dpflag)//if dpflag == true , it is data in that cacheline.
    {
        if(pcm.lines[mapped_address].write_count>=pcm.lines[mapped_address].lifetime)
        {
            wear_out_count++;
            unsigned int re_mapped_address;
            bool success=remapping(mapped_address,&re_mapped_address);//A failure block is remapped to a logical address
            if(success)
            {
                return access_line(re_mapped_address,update,deepth+1);
                //perform_access_pcm(re_mapped_address);
                //wear_leveling("start_gap");
            }
            else
            {
                return false;
            }
        }
        else
        {
            perform_access_pcm(mapped_address);
            int percent=wear_out_count/(pcm_size*0.1);
            if((pointer_printed[percent]==false)&&(wear_out_count>0)&&((wear_out_count%(pcm_size/10)==0)))
            {
                pointer_printed[percent]=true;
                print_pointer();
            }
            return true;
            //wear_leveling("start_gap");
        }
    }
    else// dp == false , it is a pointer in that line.
    {
         return access_line(pcm.lines[mapped_address].remap_address,update,deepth+1);
    }
}
bool access_address(unsigned int memory_address,bool update,int deepth)
{
    return access_line(memory_address/line_size,update,deepth);
}

unsigned int write_count_sum[pcm_size];
unsigned int overlay()//in order to reduce runing time, we overlay write count before wear-out.
{
                       //it is used for security refresh.
    while(true)
    {
        unsigned int i;
         for(i=0;i<=pivot;i++)
         {
            unsigned int xor_address;
            unsigned int match_address;
            xor_address=xor_map(i<<line_bit_number,39,6,kc);
            xor_address=xor_map(xor_address,39,6,kp);
            match_address=xor_address>>line_bit_number;
            write_count_sum[i]+=pcm.lines[match_address].write_count;
            cout<<write_count_sum[i]<<endl;
            if(write_count_sum[i]>=pcm.lines[i].lifetime)
            {
                return i;
            }
        }
        kp=kc;
        kc=rand()%0xffffffff;
    }
}
void print_pointer()
{
    unsigned int deepest_point=0;
    unsigned int i = 0;
    for(i=0;i<pcm_size;i++)
    {
       /* if(pcm.lines[i].write_count>=0)
        {
            printf("%u : %u\n",i,pcm.lines[i].write_count);
        }*/
        if(deepest_point<pcm.lines[i].point_deep)
        {
            deepest_point=pcm.lines[i].point_deep;
        }
    }

    for(i=0;i<pcm_size;i++)
    {
        if(pcm.lines[i].dpflag==true)
        {
            pointer_deepth[pcm.lines[i].point_deep]++;
        }
    }
    cout<<"\nwear-out percent: "<<fixed<<setprecision(1)<<((float)wear_out_count/(float)pcm_size)<<endl;
    for(i=0;i<=deepest_point;i++)
    {
        if(pointer_deepth[i]>0)
        {
                cout<<"pointer deepth = "<<i<<"  ; count = "<<pointer_deepth[i]<<endl;
        }
    }
    if(outfile.is_open())
    {
        outfile<<"\nwear-out percent: "<<fixed<<setprecision(1)<<((float)wear_out_count/(float)pcm_size)<<endl;
        for(i=0;i<=deepest_point;i++)
        {
            if(pointer_deepth[i]>0)
            {
                    outfile<<"pointer deepth = "<<i<<"  ; count = "<<pointer_deepth[i]<<endl;
            }
        }
    }
}
unsigned int access_from_file(char * filename)
{
    ifstream trace(filename);
    if(trace.is_open()==false)
    {
        cout<<"error:opening trace file "<<filename<<endl;
        return 0;
    }
    while(trace.eof()==false)
    {
        trace>>trace_data[trace_len];
        trace_len++;
    }
    cout<<"trace length= "<<trace_len<<endl;
    if(trace.is_open())
    {
        trace.close();
    }
    trace_len--;//in case the last one is a counter,rather than an address.

        unsigned int address;
        unsigned int i=0;
        bool successful=true;
        while(successful)
        {
            access_count++;
            address=trace_data[i];
            i=(i+1)%trace_len;

            //cout<<address<<" ";
            if((address>>line_bit_number)>pivot)//addresss larger than pivot are used for remapping and are unvisible to OS.
            {
                //cout<<"\naccessed line address "<<address/line_size<<" is larger than size of pcm : "<<pivot<<endl;
                //return access_count;
                exceed_write_count++;
                //address=address%pivot;
                continue;
            }
            successful=access_address(address,false,0);
            //if(total_write_count%1000==0)
                //cout<<" total write count: "<<total_write_count<<endl;
            if(successful==false)
            {
                cout<<"The device is wear out!"<<endl;
                break;
            }
/*
            if(crp==outer_up_limitation)
            {
                cout<<"******************************"<<endl;
                //overlay();
            }
*/
            total_write_count++;
#ifdef DEBUG
            if(total_write_count>=83886200)
            {
                cout<<"total write count = "<<total_write_count<<endl;
                int i=0;
                i++;
            }
#endif
            if(wear_leveling(wl_method)==false)
            {
                cout<<"The device is wear out!"<<endl;
                break;
            }
#ifdef DEBUG
            if(total_write_count>=83886200)
            {
                cout<<"total write count = "<<total_write_count<<endl;
                int i=0;
                i++;
            }
#endif
        }
    return access_count;
}


void output_result()
{
    unsigned int deepest_point=0;
    unsigned int i = 0;
    for(i=0;i<pcm_size;i++)
    {
        if(deepest_point<pcm.lines[i].point_deep)
        {
            deepest_point=pcm.lines[i].point_deep;
        }
    }

    for(i=0;i<pcm_size;i++)
    {
        if(pcm.lines[i].dpflag==true)
        {
            pointer_deepth[pcm.lines[i].point_deep]++;
        }
    }

    cout<<"method:"<<wl_method<<endl;
    cout<<"trace file: "<<trace<<endl;
    cout<<"pcm size(line):"<<pcm_size<<endl;
    if(strcmp(wl_method,"security_refresh")==0)
    {
        cout<<"refresh interval:"<<refresh_requency<<endl;
        cout<<"last crp:"<<(crp>>line_bit_number)<<endl;
        cout<<"refresh count: "<<refresh_count<<endl;
        cout<<"refresh round: "<<refresh_round<<endl;
    }
    cout<<"cell lifetime: "<<pcm.lines[0].lifetime<<endl;
    cout<<"deepth of the deepest point: " << deepest_point<<endl;
    cout<<"access count: "<<access_count<<endl;
    cout<<"total write count: "<<total_write_count<<endl;
    cout<<"wear-out count: "<<wear_out_count<<endl;
    cout<<"exceeded write count: "<<exceed_write_count<<endl;
    cout<<"normal space : backup space = "<<pivot<<" : "<< (pcm_size-pivot)<<" backup space percent:"<<setprecision(2)<<(float)(pcm_size-pivot)/(float)pcm_size<<endl;
    cout<<"\nwear-out percent: "<<fixed<<setprecision(4)<<((float)wear_out_count/(float)pcm_size)<<endl;

    for(i=0;i<=deepest_point;i++)
    {
        if(pointer_deepth[i]>0)
        {
                cout<<"pointer deepth = "<<i<<"  ; count = "<<pointer_deepth[i]<<endl;
        }
    }
    if(outfile.is_open())
    {
        outfile<<"method:"<<wl_method<<endl;
        outfile<<"trace file: "<<trace<<endl;
        outfile<<"pcm size(line):"<<pcm_size<<endl;
        if(strcmp(wl_method,"security_refresh")==0)
        {
            outfile<<"refresh interval:"<<refresh_requency<<endl;
            outfile<<"last crp:"<<(crp>>line_bit_number)<<endl;
            outfile<<"refresh count: "<<refresh_count<<endl;
            outfile<<"refresh round: "<<refresh_round<<endl;
        }
        outfile<<"cell lifetime: "<<pcm.lines[0].lifetime<<endl;
        outfile<<"deepth of the deepest point: " << deepest_point<<endl;
        outfile<<"access count: "<<access_count<<endl;
        outfile<<"total write count: "<<total_write_count<<endl;
        outfile<<"wear-out count: "<<wear_out_count<<endl;
        outfile<<"exceeded write count: "<<exceed_write_count<<endl;
        outfile<<"normal space : backup space = "<<pivot<<" : "<< (pcm_size-pivot)<<" backup space percent:"<<setprecision(2)<<(float)(pcm_size-pivot)/(float)pcm_size<<endl;
        outfile<<"\nwear-out percent: "<<fixed<<setprecision(4)<<((float)wear_out_count/(float)pcm_size)<<endl;
        for(i=0;i<=deepest_point;i++)
        {
            if(pointer_deepth[i]>0)
            {
                outfile<<"pointer deepth = "<<i<<"  ; count = "<<pointer_deepth[i]<<endl;
            }
        }
        outfile<<"=============================================================================="<<endl;
        outfile<<endl;
    }
}
int main()
{
    cout << "WLWO begins ... " << endl;

    if(outfile.is_open()==false)
    {
        cout<<"error opening result file:"<<result_path<<endl;
        cout<<"result cannot been saved!"<<endl;
    }
    if(strcmp(wl_method,"start_gap")==false)//random address for start_gap
    {
        ifstream random_map_file(random);
        if(random_map_file.is_open()==false)
        {
            cout<<"Error: opening 24bits_randomized_addr.dat"<<endl;
            return 0;
        }
        unsigned int i=0;
        while(random_map_file.eof()==0)
        {
            random_map_file>>random_map[i];
            i++;
        }
        if(random_map_file.is_open())
        {
            random_map_file.close();
        }
    }

    access_from_file(trace);
    output_result();
    if(outfile.is_open())
    {
        outfile.close();
    }
    cout << "WLWO ends!" << endl;
    return 0;
}
