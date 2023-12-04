#include "ProcessMap.h"

VOID
ProcessMap::Initialize()
{
    RtlInitializeGenericTableAvl(&m_table, &_CompareRoutine, &_AllocateRoutine, &_FreeRoutine,
        NULL);
    ExInitializeFastMutex(&m_mutex);
}

VOID
ProcessMap::Clear()
{
    PVOID pCurrentElement = NULL;
    while ((pCurrentElement = RtlEnumerateGenericTableAvl(&m_table, TRUE)) != NULL)
    {
        RtlDeleteElementGenericTableAvl(&m_table, pCurrentElement);
    }
}

BOOLEAN
ProcessMap::ProcessBelongsToHandler(
    _In_ PEPROCESS process,
    _In_ DWORD handler
)
{
    PPROCESS_HANDLER_INFORMATION pInfo = (*this)[process];

    if (pInfo == NULL)
    {
        return FALSE;
    }

    return pInfo->Handler == handler;
}

NTSTATUS
ProcessMap::GetProcessHandler(
    _In_ PEPROCESS process,
    _Out_ DWORD* handler
)
{
    if (handler == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    PPROCESS_HANDLER_INFORMATION pInfo = (*this)[process];

    if (pInfo == NULL)
    {
        return STATUS_NOT_FOUND;
    }

    *handler = pInfo->Handler;

    return STATUS_SUCCESS;
}

NTSTATUS
ProcessMap::GetProcessHandler(
    _In_ PEPROCESS process,
    _Out_ PPROCESS_HANDLER_INFORMATION handlerInformation
)
{
    if (handlerInformation == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    PPROCESS_HANDLER_INFORMATION pInfo = (*this)[process];

    if (pInfo == NULL)
    {
        return STATUS_NOT_FOUND;
    }

    *handlerInformation = *pInfo;

    return STATUS_SUCCESS;
}

NTSTATUS
ProcessMap::RegisterProcessHandler(
    _In_ PEPROCESS process,
    _In_ DWORD handler
)
{
    _NODE node;
    memset(&node, 0, sizeof(node));

    node.Key = process;
    node.Value.Handler = handler;

    BOOLEAN bNewEntry = FALSE;

    _PNODE pInsertedNode = (_PNODE)
        RtlInsertElementGenericTableAvl(&m_table, &node, sizeof(node), &bNewEntry);

    if (pInsertedNode == FALSE)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (!bNewEntry)
    {
        return STATUS_ALREADY_REGISTERED;
    }

    ASSERT(pInsertedNode->Value.Handler == handler);

    return STATUS_SUCCESS;
}

NTSTATUS
ProcessMap::SwitchProcessHandler(
    _In_ PEPROCESS process,
    _In_ DWORD newHandler
)
{
    PPROCESS_HANDLER_INFORMATION pInfo = (*this)[process];
    BOOLEAN bHasInternalProvider = TRUE;

    if (pInfo == NULL)
    {
        NTSTATUS status = RegisterProcessHandler(process, (DWORD)-1);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        pInfo = (*this)[process];
        bHasInternalProvider = FALSE;

        ASSERT(pInfo != NULL);
    }

    // TODO: This is mainly a Monika restriction, not the data structure's;
    // move this check to monika.cpp instead?
    if (pInfo->HasParentHandler)
    {
        return STATUS_NOT_IMPLEMENTED;
    }

    pInfo->HasParentHandler = TRUE;
    pInfo->HasInternalParentHandler = bHasInternalProvider;
    pInfo->ParentHandler = pInfo->Handler;
    pInfo->Handler = newHandler;

    return STATUS_SUCCESS;
}

NTSTATUS
ProcessMap::UnregisterProcess(
    _In_ PEPROCESS process
)
{
    _NODE node;

    // Only this member is needed.
    node.Key = process;

    if (RtlDeleteElementGenericTableAvl(&m_table, &node) == FALSE)
    {
        return STATUS_NOT_FOUND;
    }

    return STATUS_SUCCESS;
}

PPROCESS_HANDLER_INFORMATION
ProcessMap::operator[](
    _In_ PEPROCESS key
)
{
    _NODE node
    {
        .Key = key
    };

    PVOID pBuffer = RtlLookupElementGenericTableAvl(&m_table, &node);

    if (pBuffer == NULL)
    {
        return NULL;
    }

    return &((_PNODE)pBuffer)->Value;
}

RTL_GENERIC_COMPARE_RESULTS
ProcessMap::_CompareRoutine(
    _In_ PRTL_AVL_TABLE Table,
    _In_ PVOID FirstStruct,
    _In_ PVOID SecondStruct
)
{
    UNREFERENCED_PARAMETER(Table);

    _PNODE FirstNode = (_PNODE)FirstStruct;
    _PNODE SecondNode = (_PNODE)SecondStruct;

    if (FirstNode->Key == SecondNode->Key)
    {
        return GenericEqual;
    }
    else if ((ULONG_PTR)FirstNode->Key > (ULONG_PTR)SecondNode->Key)
    {
        return GenericGreaterThan;
    }
    else
    {
        return GenericLessThan;
    }
}

PVOID
ProcessMap::_AllocateRoutine(
    _In_ PRTL_AVL_TABLE Table,
    _In_ CLONG ByteSize
)
{
    UNREFERENCED_PARAMETER(Table);

    return ExAllocatePoolZero(PagedPool, ByteSize, 'PMAP');
}

VOID
ProcessMap::_FreeRoutine(
    _In_ PRTL_AVL_TABLE Table,
    _In_ PVOID Buffer
)
{
    UNREFERENCED_PARAMETER(Table);

    return ExFreePoolWithTag(Buffer, 'PMAP');
}
