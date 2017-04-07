/*
Copyright (c) 2014-2017, Wenqi Chen

Shanghai Mifu Infotech Co., Ltd
B112-113, IF Industrial Park, 508 Chunking Road, Shanghai 201103, China


All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS AS IS AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


*/

#include "duct.h"
#include "duct_pre_xnu.h"
#include "duct_kern_task.h"
#include "duct_kern_zalloc.h"
// #include "duct_machine_routines.h"

#include <kern/mach_param.h>
#include <kern/task.h>
#include <kern/locks.h>
#include <kern/ipc_tt.h>

#include "duct_post_xnu.h"


task_t            kernel_task;
zone_t            task_zone;
lck_attr_t      task_lck_attr;
lck_grp_t       task_lck_grp;
lck_grp_attr_t  task_lck_grp_attr;


void duct_task_init (void)
{
#if defined (__DARLING__)
#else
        lck_grp_attr_setdefault(&task_lck_grp_attr);
        lck_grp_init(&task_lck_grp, "task", &task_lck_grp_attr);
        lck_attr_setdefault(&task_lck_attr);
#endif
        lck_mtx_init(&tasks_threads_lock, &task_lck_grp, &task_lck_attr);

#if defined (__DARLING__)
#else
    #if CONFIG_EMBEDDED
        lck_mtx_init(&task_watch_mtx, &task_lck_grp, &task_lck_attr);
    #endif /* CONFIG_EMBEDDED */
#endif


        task_zone = duct_zinit(
                sizeof(struct task),
                task_max * sizeof(struct task),
                TASK_CHUNK * sizeof(struct task),
                "tasks");

        duct_zone_change(task_zone, Z_NOENCRYPT, TRUE);

        // init_task_ledgers();
        /*
         * Create the kernel task as the first task.
         */
    #ifdef __LP64__
        if (duct_task_create_internal(TASK_NULL, FALSE, TRUE, &kernel_task) != KERN_SUCCESS)
    #else
        if (duct_task_create_internal(TASK_NULL, FALSE, FALSE, &kernel_task) != KERN_SUCCESS)
    #endif
            panic("task_init\n");


#if defined (__DARLING__)
#else
        vm_map_deallocate(kernel_task->map);
        kernel_task->map = kernel_map;
        lck_spin_init(&dead_task_statistics_lock, &task_lck_grp, &task_lck_attr);
#endif

}

void duct_task_destroy(task_t task)
{
        if (task == TASK_NULL) {
                return;
        }
        
        ipc_space_terminate (task->itk_space);
        task_deallocate(task);

}

kern_return_t duct_task_create_internal (task_t parent_task, boolean_t inherit_memory, boolean_t is_64bit, task_t * child_task)
{
        task_t            new_task;

    #if defined (__DARLING__)
    #else
        vm_shared_region_t    shared_region;

        ledger_t        ledger = NULL;
    #endif

        new_task = (task_t) duct_zalloc(task_zone);

        // printk (KERN_NOTICE "task create internal's new task: 0x%x", (unsigned int) new_task);

        if (new_task == TASK_NULL)
            return(KERN_RESOURCE_SHORTAGE);

        /* one ref for just being alive; one for our caller */
        new_task->ref_count = 2;

        // /* allocate with active entries */
        // assert(task_ledger_template != NULL);
        // if ((ledger = ledger_instantiate(task_ledger_template,
        //         LEDGER_CREATE_ACTIVE_ENTRIES)) == NULL) {
        //     zfree(task_zone, new_task);
        //     return(KERN_RESOURCE_SHORTAGE);
        // }
        // new_task->ledger = ledger;


    #if defined (__DARLING__)
    #else
        // /* if inherit_memory is true, parent_task MUST not be NULL */
        // if (inherit_memory)
        //     new_task->map = vm_map_fork(ledger, parent_task->map);
        // else
        //     new_task->map = vm_map_create(pmap_create(ledger, 0, is_64bit),
        //             (vm_map_offset_t)(VM_MIN_ADDRESS),
        //             (vm_map_offset_t)(VM_MAX_ADDRESS), TRUE);
        //
        // /* Inherit memlock limit from parent */
        // if (parent_task)
        //     vm_map_set_user_wire_limit(new_task->map, (vm_size_t)parent_task->map->user_wire_limit);
    #endif
    //
        lck_mtx_init(&new_task->lock, &task_lck_grp, &task_lck_attr);
        queue_init(&new_task->threads);
        new_task->suspend_count = 0;
        new_task->thread_count = 0;
        new_task->active_thread_count = 0;
        new_task->user_stop_count = 0;
        new_task->role = TASK_UNSPECIFIED;
        new_task->active = TRUE;
        new_task->halting = FALSE;
        new_task->user_data = NULL;
        new_task->faults = 0;
        new_task->cow_faults = 0;
        new_task->pageins = 0;
        new_task->messages_sent = 0;
        new_task->messages_received = 0;
        new_task->syscalls_mach = 0;
        new_task->priv_flags = 0;
        new_task->syscalls_unix=0;
        new_task->c_switch = new_task->p_switch = new_task->ps_switch = 0;
        new_task->taskFeatures[0] = 0;                /* Init task features */
        new_task->taskFeatures[1] = 0;                /* Init task features */

        zinfo_task_init(new_task);

    // #ifdef MACH_BSD
    //     new_task->bsd_info = NULL;
    // #endif /* MACH_BSD */

    // #if defined(__i386__) || defined(__x86_64__)
    //     new_task->i386_ldt = 0;
    //     new_task->task_debug = NULL;
    // #endif


        queue_init(&new_task->semaphore_list);
        queue_init(&new_task->lock_set_list);
        new_task->semaphores_owned = 0;
        new_task->lock_sets_owned = 0;

    #if CONFIG_MACF_MACH
        new_task->label = labelh_new(1);
        mac_task_label_init (&new_task->maclabel);
    #endif

        ipc_task_init(new_task, parent_task);

        new_task->total_user_time = 0;
        new_task->total_system_time = 0;

        new_task->vtimers = 0;

        new_task->shared_region = NULL;

        new_task->affinity_space = NULL;
    //
    // #if CONFIG_COUNTERS
    //     new_task->t_chud = 0U;
    // #endif
    //
    //     new_task->pidsuspended = FALSE;
    //     new_task->frozen = FALSE;
    //     new_task->rusage_cpu_flags = 0;
    //     new_task->rusage_cpu_percentage = 0;
    //     new_task->rusage_cpu_interval = 0;
    //     new_task->rusage_cpu_deadline = 0;
    //     new_task->rusage_cpu_callt = NULL;
    //     new_task->proc_terminate = 0;
    // #if CONFIG_EMBEDDED
    //     queue_init(&new_task->task_watchers);
    //     new_task->appstate = TASK_APPSTATE_ACTIVE;
    //     new_task->num_taskwatchers  = 0;
    //     new_task->watchapplying  = 0;
    // #endif /* CONFIG_EMBEDDED */

    //     new_task->uexc_range_start = new_task->uexc_range_size = new_task->uexc_handler = 0;
    //
        if (parent_task != TASK_NULL) {
                new_task->sec_token     = parent_task->sec_token;
                new_task->audit_token   = parent_task->audit_token;

                // printk (KERN_NOTICE "- new task audit[5]: 0x%x\n", new_task->audit_token.val[5]);

            #if defined (__DARLING__)
            #else
                /* inherit the parent's shared region */
                // shared_region = vm_shared_region_get(parent_task);
                // vm_shared_region_set(new_task, shared_region);
            #endif
    //
    //         if(task_has_64BitAddr(parent_task))
    //             task_set_64BitAddr(new_task);
    //         new_task->all_image_info_addr = parent_task->all_image_info_addr;
    //         new_task->all_image_info_size = parent_task->all_image_info_size;
    //
    // #if defined (__DARLING__)
    // #else
    // // #if defined(__i386__) || defined(__x86_64__)
    // //         if (inherit_memory && parent_task->i386_ldt)
    // //             new_task->i386_ldt = user_ldt_copy(parent_task->i386_ldt);
    // // #endif
    //         if (inherit_memory && parent_task->affinity_space)
    //             task_affinity_create(parent_task, new_task);
    // #endif
    //
    // #if defined (__DARLING__)
    // #else
    //         new_task->pset_hint = parent_task->pset_hint = task_choose_pset(parent_task);
    // #endif
    //
    //         new_task->policystate = parent_task->policystate;
    //         /* inherit the self action state */
    //         new_task->appliedstate = parent_task->appliedstate;
    //         new_task->ext_policystate = parent_task->ext_policystate;
    // #if NOTYET
    //         /* till the child lifecycle is cleared do not inherit external action */
    //         new_task->ext_appliedstate = parent_task->ext_appliedstate;
    // #else
    //         new_task->ext_appliedstate = default_task_null_policy;
    // #endif
        }
        else {
                new_task->sec_token     = KERNEL_SECURITY_TOKEN;
                new_task->audit_token   = KERNEL_AUDIT_TOKEN;

    // // #ifdef __LP64__
    // //         if(is_64bit)
    // //             task_set_64BitAddr(new_task);
    // // #endif
    //         new_task->all_image_info_addr = (mach_vm_address_t)0;
    //         new_task->all_image_info_size = (mach_vm_size_t)0;
    //
    //         new_task->pset_hint = PROCESSOR_SET_NULL;
    //         new_task->policystate = default_task_proc_policy;
    //         new_task->ext_policystate = default_task_proc_policy;
    //         new_task->appliedstate = default_task_null_policy;
    //         new_task->ext_appliedstate = default_task_null_policy;
        }

    //
    //     if (kernel_task == TASK_NULL) {
    //         new_task->priority = BASEPRI_KERNEL;
    //         new_task->max_priority = MAXPRI_KERNEL;
    //     }
    //     else {
    //         new_task->priority = BASEPRI_DEFAULT;
    //         new_task->max_priority = MAXPRI_USER;
    //     }
    //
    //     bzero(&new_task->extmod_statistics, sizeof(new_task->extmod_statistics));
    //     new_task->task_timer_wakeups_bin_1 = new_task->task_timer_wakeups_bin_2 = 0;
    //
    //     lck_mtx_lock(&tasks_threads_lock);
    //     queue_enter(&tasks, new_task, task_t, tasks);
    //     tasks_count++;
    //     lck_mtx_unlock(&tasks_threads_lock);
    //
        #if defined (__DARLING__)
        #else
            // if (vm_backing_store_low && parent_task != NULL)
            //     new_task->priv_flags |= (parent_task->priv_flags&VM_BACKING_STORE_PRIV);
        #endif

        ipc_task_enable (new_task);

        *child_task = new_task;
        return(KERN_SUCCESS);
}


#undef duct_current_task
task_t duct_current_task (void);

task_t duct_current_task (void)
{
        return (current_task_fast ());
}