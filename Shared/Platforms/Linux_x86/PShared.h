////////////////////////////////////////////////////////////////////////////////
//                                                                             /
// 2012-2020 (c) Baical                                                        /
//                                                                             /
// This library is free software; you can redistribute it and/or               /
// modify it under the terms of the GNU Lesser General Public                  /
// License as published by the Free Software Foundation; either                /
// version 3.0 of the License, or (at your option) any later version.          /
//                                                                             /
// This library is distributed in the hope that it will be useful,             /
// but WITHOUT ANY WARRANTY; without even the implied warranty of              /
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU           /
// Lesser General Public License for more details.                             /
//                                                                             /
// You should have received a copy of the GNU Lesser General Public            /
// License along with this library.                                            /
//                                                                             /
////////////////////////////////////////////////////////////////////////////////
#ifndef PSHARED_H
#define PSHARED_H


////////////////////////////////////////////////////////////////////////////////
//Headers dependencies:
#include <unistd.h>
#include <fcntl.h>           // For O_* constants
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>        // For mode constants

#define SHARED_NAME_FORMAT_STRING "/P7_Type%d_PID%d_Text%s"
#define SHARED_NAME_LEN           (strlen(SHARED_NAME_FORMAT_STRING) + 64)
#define SHARED_SEM_NULL           SEM_FAILED

////////////////////////////////////////////////////////////////////////////////
//CShared
class CShared
{
    struct sShared
    {
        int     iMFD;
        sem_t  *hSemaphore;
        size_t  szName;
        char   *pName;
        char   *pSemName;
        char   *pMemName;
    };

    enum eType
    {
        ETYPE_MUTEX   = 0,
        ETYPE_FILE       ,
        ETYPE_MAX
    };

public:
    typedef sShared *hShared;
    typedef sem_t   *hSem;

    enum eLock
    {
        E_OK         ,
        E_TIMEOUT    ,
        E_ERROR      ,
        E_NOT_EXISTS
    };


    ////////////////////////////////////////////////////////////////////////////
    //CShared::Create
    static tBOOL Create(CShared::hShared *o_pHandle,
                        const tXCHAR     *i_pName,
                        const tUINT8     *i_pData,
                        tUINT16           i_wSize
                       )
    {
        hShared  l_pShared  = NULL;
        tBOOL    l_bResult  = TRUE;
        tBOOL    l_bRelease = FALSE;
        void    *l_pBuffer  = NULL;

        if (    (NULL == i_pName)
             || (NULL == i_pData)
             || (0    >= i_wSize)
             || (NULL == o_pHandle)
           )
        {
            l_bResult = FALSE;
            goto l_lblExit;
        }

        l_pShared = (hShared)malloc(sizeof(sShared));
        if (NULL == l_pShared)
        {
            l_bResult = FALSE;
            goto l_lblExit;
        }

        memset(l_pShared, 0, sizeof(sShared));
        l_pShared->hSemaphore = SHARED_SEM_NULL;
        l_pShared->iMFD       = -1;

        l_pShared->szName    = strlen(i_pName) + SHARED_NAME_LEN;
        l_pShared->pName     = strdup(i_pName);
        l_pShared->pSemName  = (char*)malloc(l_pShared->szName);
        l_pShared->pMemName  = (char*)malloc(l_pShared->szName);

        if (    (NULL == l_pShared->pSemName)
             || (NULL == l_pShared->pMemName)
             || (NULL == l_pShared->pName)
           )
        {
            l_bResult = FALSE;
            goto l_lblExit;
        }

        ////////////////////////////////////////////////////////////////////////
        //create ssemaphore and own it
        Create_Name(l_pShared->pSemName, l_pShared->szName, ETYPE_MUTEX, i_pName);

        l_pShared->hSemaphore = sem_open(l_pShared->pSemName, O_CREAT | O_EXCL, 0666, 0);
        if (SHARED_SEM_NULL == l_pShared->hSemaphore)
        {
            l_bResult  = FALSE;
            goto l_lblExit;
        }

        l_bRelease = TRUE;

        ////////////////////////////////////////////////////////////////////////
        //share memory
        Create_Name(l_pShared->pMemName, l_pShared->szName, ETYPE_FILE, i_pName);

        l_pShared->iMFD = shm_open(l_pShared->pMemName,
                                   O_RDWR | O_CREAT | O_EXCL,
                                   0666
                                  );
        if (0 > l_pShared->iMFD)
        {
            l_bResult  = FALSE;
            goto l_lblExit;
        }

        if (0 != ftruncate(l_pShared->iMFD, (off_t)i_wSize))
        {
            l_bResult  = FALSE;
            goto l_lblExit;
        }

        l_pBuffer = mmap(0,
                         (size_t)i_wSize,
                         PROT_READ | PROT_WRITE,
                         MAP_SHARED,
                         l_pShared->iMFD,
                         0
                        );

        if (    (NULL == l_pBuffer)
             || (MAP_FAILED == l_pBuffer)
           )
        {
            l_bResult  = FALSE;
            goto l_lblExit;
        }

        *o_pHandle = (CShared::hShared)l_pShared;

        memcpy(l_pBuffer, i_pData, (size_t)i_wSize);

        if (0 != munmap(l_pBuffer, (size_t)i_wSize))
        {
        }


    l_lblExit:
        if (l_bRelease)
        {
            sem_post(l_pShared->hSemaphore);
        }

        if (FALSE == l_bResult)
        {
            Close(l_pShared);
            l_pShared = NULL;

            if (o_pHandle)
            {
                *o_pHandle = NULL;
            }
        }

        return l_bResult;
    }//CShared::Create


    ////////////////////////////////////////////////////////////////////////////
    //Read
    static tBOOL Read(const tXCHAR  *i_pName,
                      void          *o_pData,
                      tUINT16        i_wSize
                     )
    {
        tBOOL       l_bResult  = TRUE;
        size_t      l_szName   = 0;
        char       *l_pName    = NULL;
        void       *l_pBuffer  = NULL;
        int         l_iMFD     = -1;
        struct stat l_sStat;


        if (    (NULL == i_pName)
             || (NULL == o_pData)
             || (0    >= i_wSize)
           )
        {
            l_bResult = FALSE;
            goto l_lblExit;
        }

        l_szName = strlen(i_pName) + SHARED_NAME_LEN;
        l_pName = (char*)malloc(l_szName);

        if (NULL == l_pName)
        {
            l_bResult = FALSE;
            goto l_lblExit;
        }

        ////////////////////////////////////////////////////////////////////////////
        //open shared memory object
        Create_Name(l_pName, l_szName, ETYPE_FILE, i_pName);

        l_iMFD = shm_open(l_pName, O_RDONLY, 0444);

        if (0 > l_iMFD)
        {
            l_bResult  = FALSE;
            goto l_lblExit;
        }

        memset(&l_sStat, 0, sizeof(l_sStat));
        if (-1 == fstat(l_iMFD, &l_sStat))
        {
            l_bResult = FALSE;
            goto l_lblExit;
        }

        if ((size_t)l_sStat.st_size > (size_t)i_wSize)
        {
            l_bResult = FALSE;
            goto l_lblExit;
        }

        l_pBuffer = mmap(0,
                         (size_t)l_sStat.st_size,
                         PROT_READ,
                         MAP_SHARED,
                         l_iMFD,
                         0
                        );

        if (    (NULL == l_pBuffer)
             || (MAP_FAILED == l_pBuffer)
           )
        {
            l_bResult  = FALSE;
            goto l_lblExit;
        }

        memcpy(o_pData, l_pBuffer, (size_t)l_sStat.st_size);

        if (0 == munmap(l_pBuffer, (size_t)l_sStat.st_size))
        {
            l_pBuffer = NULL;
        }


    l_lblExit:
        if (l_pName)
        {
            free(l_pName);
            l_pName = NULL;
        }

        if (0 <= l_iMFD)
        {
            close(l_iMFD);
            l_iMFD = -1;
        }

        return l_bResult;
    }//Read


    ////////////////////////////////////////////////////////////////////////////
    //Write
    static tBOOL Write(const tXCHAR  *i_pName,
                       const tUINT8  *i_pData,
                       tUINT16        i_wSize
                      )
    {
        tBOOL       l_bResult  = TRUE;
        size_t      l_szName   = 0;
        char       *l_pName    = NULL;
        void       *l_pBuffer  = NULL;
        int         l_iMFD     = -1;
        struct stat l_sStat;


        if (    (NULL == i_pName)
             || (NULL == i_pData)
             || (0    >= i_wSize)
           )
        {
            l_bResult = FALSE;
            goto l_lblExit;
        }

        l_szName = strlen(i_pName) + SHARED_NAME_LEN;
        l_pName = (char*)malloc(l_szName);

        if (NULL == l_pName)
        {
            l_bResult = FALSE;
            goto l_lblExit;
        }

        ////////////////////////////////////////////////////////////////////////////
        //open shared memory object
        Create_Name(l_pName, l_szName, ETYPE_FILE, i_pName);

        l_iMFD = shm_open(l_pName, O_RDWR, 0666);

        if (0 > l_iMFD)
        {
            l_bResult  = FALSE;
            goto l_lblExit;
        }

        memset(&l_sStat, 0, sizeof(l_sStat));
        if (-1 == fstat(l_iMFD, &l_sStat))
        {
            l_bResult = FALSE;
            goto l_lblExit;
        }

        if ((size_t)l_sStat.st_size < (size_t)i_wSize)
        {
            l_bResult = FALSE;
            goto l_lblExit;
        }

        l_pBuffer = mmap(0,
                         (size_t)l_sStat.st_size,
                         PROT_READ | PROT_WRITE,
                         MAP_SHARED,
                         l_iMFD,
                         0
                        );

        if (    (NULL == l_pBuffer)
             || (MAP_FAILED == l_pBuffer)
           )
        {
            l_bResult  = FALSE;
            goto l_lblExit;
        }

        memcpy(l_pBuffer, i_pData, (size_t)i_wSize);

        if (0 == munmap(l_pBuffer, (size_t)l_sStat.st_size))
        {
            l_pBuffer = NULL;
        }


    l_lblExit:
        if (l_pName)
        {
            free(l_pName);
            l_pName = NULL;
        }

        if (0 <= l_iMFD)
        {
            close(l_iMFD);
            l_iMFD = -1;
        }

        return l_bResult;
    }//Write


    ////////////////////////////////////////////////////////////////////////////
    //Lock
    static eLock Lock(const tXCHAR  *i_pName, CShared::hSem &o_rSem, tUINT32 i_dwTimeout_ms)
    {
        eLock       l_eReturn  = CShared::E_TIMEOUT;
        size_t      l_szName   = 0;
        char       *l_pName    = NULL;
        const int   l_i1ms     = 1000;
        tINT64      l_llWait   = (tINT64)i_dwTimeout_ms * 1000LL;
        sem_t      *l_hSem     = SHARED_SEM_NULL;

        o_rSem = SHARED_SEM_NULL;

        if (NULL == i_pName)
        {
            l_eReturn = CShared::E_ERROR;
            goto l_lblExit;
        }

        l_szName = strlen(i_pName) + SHARED_NAME_LEN;
        l_pName = (char*)malloc(l_szName);

        if (NULL == l_pName)
        {
            l_eReturn = CShared::E_ERROR;
            goto l_lblExit;
        }


        ////////////////////////////////////////////////////////////////////////
        //open semaphore
        Create_Name(l_pName, l_szName, ETYPE_MUTEX, i_pName);

        l_hSem = sem_open(l_pName, 0);
        if (SHARED_SEM_NULL == l_hSem)
        {
            l_eReturn = CShared::E_NOT_EXISTS;
            goto l_lblExit;
        }

        l_eReturn = CShared::E_TIMEOUT;

        while (0 < l_llWait)
        {
            if (0 == sem_trywait(l_hSem))
            {
                l_eReturn = CShared::E_OK;
                break;
            }
            else
            {
                usleep(l_i1ms); //10 ms
                l_llWait -= l_i1ms;
            }
        }

    l_lblExit:
        if (l_pName)
        {
            free(l_pName);
            l_pName = NULL;
        }

        if (SHARED_SEM_NULL != l_hSem)
        {
            if (CShared::E_OK == l_eReturn)
            {
                o_rSem = l_hSem;
            }
            else
            {
                sem_close(l_hSem);
                l_hSem = SHARED_SEM_NULL;
            }
        }

        return l_eReturn;
    }//Lock

    ////////////////////////////////////////////////////////////////////////////
    //UnLock
    static eLock UnLock(CShared::hSem &io_rSem)
    {
        eLock l_eReturn  = CShared::E_ERROR;

        if (SHARED_SEM_NULL == io_rSem)
        {
            l_eReturn = CShared::E_NOT_EXISTS;
            goto l_lblExit;
        }

        if (0 == sem_post(io_rSem))
        {
            l_eReturn = CShared::E_OK;
        }

        sem_close(io_rSem);
        io_rSem = SHARED_SEM_NULL;

    l_lblExit:
        return l_eReturn;
    }//UnLock

    ////////////////////////////////////////////////////////////////////////////
    //GetSemName
    static const tXCHAR* GetName(hShared i_pShared)
    {
        if (NULL == i_pShared)
        {
            return FALSE;
        }

        return i_pShared->pName;
    }


    ////////////////////////////////////////////////////////////////////////////
    //Close
    static tBOOL Close(hShared i_pShared)
    {
        if (NULL == i_pShared)
        {
            return FALSE;
        }

        //if (SHARED_SEM_NULL != i_pShared->hSemaphore)
        //{
        //    sem_wait(i_pShared->hSemaphore);
        //}

        if (0 <= i_pShared->iMFD)
        {
            close(i_pShared->iMFD);
            i_pShared->iMFD = -1;
        }

        if (i_pShared->pMemName)
        {
            shm_unlink(i_pShared->pMemName);
        }

        if (SHARED_SEM_NULL != i_pShared->hSemaphore)
        {
            int l_iRes = -1;
            l_iRes = sem_close(i_pShared->hSemaphore);
            i_pShared->hSemaphore = SHARED_SEM_NULL;
            UNUSED_ARG(l_iRes);
        }

        if (i_pShared->pSemName)
        {
            sem_unlink(i_pShared->pSemName);
            free(i_pShared->pSemName);
            i_pShared->pSemName = NULL;
        }

        if (i_pShared->pMemName)
        {
            free(i_pShared->pMemName);
            i_pShared->pMemName = NULL;
        }

        if (i_pShared->pName)
        {
            free(i_pShared->pName);
            i_pShared->pName = NULL;
        }

        i_pShared->szName = 0;

        free(i_pShared);
        i_pShared = NULL;

        return TRUE;
    }//Close

    ////////////////////////////////////////////////////////////////////////////
    //UnLink
    static tBOOL UnLink(const tXCHAR  *i_pName)
    {
        tBOOL    l_bReturn  = TRUE;
        size_t   l_szName   = 0;
        char    *l_pName    = NULL;

        if (NULL == i_pName)
        {
            l_bReturn = FALSE;
            goto l_lblExit;
        }

        l_szName = strlen(i_pName) + SHARED_NAME_LEN;
        l_pName = (char*)malloc(l_szName);

        if (NULL == l_pName)
        {
            l_bReturn = FALSE;
            goto l_lblExit;
        }

        Create_Name(l_pName, l_szName, ETYPE_MUTEX, i_pName);
        if (0 != sem_unlink(l_pName))
        {
            l_bReturn = FALSE;
        }

        Create_Name(l_pName, l_szName, ETYPE_FILE, i_pName);
        if (0 != shm_unlink(l_pName))
        {
            l_bReturn = FALSE;
        }

    l_lblExit:
        if (l_pName)
        {
            free(l_pName);
            l_pName = NULL;
        }
        return l_bReturn;
    }//UnLink

private:
    ////////////////////////////////////////////////////////////////////////////
    //Create_Name
    static tBOOL Create_Name(tXCHAR        *o_pName,
                             size_t         i_szName,
                             eType          i_eType,
                             const tXCHAR  *i_pPostfix
                            )
    {
        if (    (NULL == o_pName)
             || (16 >= i_szName)
             || (ETYPE_MAX <= i_eType)
             || (NULL == i_pPostfix)
           )
        {
            return FALSE;
        }

        snprintf(o_pName, i_szName, SHARED_NAME_FORMAT_STRING, i_eType, getpid(), i_pPostfix);

        return TRUE;
    }//Create_Name
};


#endif //PSHARED_H
