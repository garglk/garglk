#include "lists.h"

#include "syserr.h"

void initArray(void *array) {
	implementationOfSetEndOfArray((Aword *)array);
}

/* How to know we are at end of a table or array, first Aword == EOF */
void implementationOfSetEndOfArray(Aword *adr)
{
	*adr = EOF;
}


bool implementationOfIsEndOfList(Aword *adr)
{
	return *adr == EOF;
}

int lengthOfArrayImplementation(void *array_of_any_type, int element_size_in_bytes) {
    int length;
    int element_size = element_size_in_bytes/sizeof(Aword);
    Aword *array = (Aword *)array_of_any_type;
    if (array == NULL)
      syserr("Taking length of NULL array");
    for (length = 0; !isEndOfArray(&array[length*element_size]); length++)
        ;
    return length;
}

void addElementImplementation(void *array_of_any_type, void *element, int element_size_in_bytes) {
	Aword *array = (Aword *)array_of_any_type;
	int length = lengthOfArray(array);
	int element_size_in_words = element_size_in_bytes/sizeof(Aword);
	memcpy(&array[length*element_size_in_words], element, element_size_in_bytes);
	setEndOfArray(&array[(length+1)*element_size_in_words]);
}
