#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <sys/stat.h>
#include <math.h>
	#ifdef _WIN32
		#include <windows.h>
	#elif _WIN64
		#include <windows.h>
	#endif

#define RECOMMENDATION_FILE_NAME "RecomendationDataSet.csv"
#define TOKEN_DELIMITERS ";\n"
#define CSV_SEPERATOR ';'									//benim bilgisayarimda csvden okuma yaparken ;(noktali virgul) karakteriyle ayrildigi icin olusturdum.
#define BUFFER_SIZE 1024									//kitap sayisinin artmasina bagli olarak bunun boyutunu artirmak gerekebilir. her bir satir icin max karakter sayisini temsil ediyor.
#define MAX_WORD_SIZE 32									//max kullanici adi uzunlugunu temsil ediyor.
#define BOOK_SUGGESTIONS_DIRECTORY "Book Suggestions"		//kitap onerileri icin olusturulacak olan .csv uzantýlý dosyalarýn konulacagi dosya adi, bir kereye mahsus olusturulur.

//Kullanici verisi tutulan veri yapisi
typedef struct
{
	char* user_name;
	int* book_ratings;
}USER_DATA;

//bir kullaniciya en benzer k kullanici bulunurken bu veri yapisinda tutuluyor.
typedef struct
{
	int user_index;
	double similarity;
}SIMILARITY_DATA;

//verilen kullaniciya önerilen kitap indisi ve tahmin edilen puan bu veri yapisinda tutuluyor.
typedef struct
{
	int book_index;
	double predicted_rating;
}BOOK_SUGGESTION_DATA;

//Kitaplari dosyadan okuyup, kitap isimleri matrisine yerlestiren fonksiyon.
char** populateBookNamesArray(FILE* fp, int* book_count)
{
	char** book_names; //kitap isimleri matrisi
	char line[BUFFER_SIZE]; //dosyadan okunan satir bu degiskene alinir
	char* token; //alinan stringi strtok ile tokenlara bolerken kullanilan degisken

	*book_count = 0;
	book_names = (char**)calloc(1, sizeof(char*));

	fgets(line, BUFFER_SIZE, fp);
	strtok(line, TOKEN_DELIMITERS);
	//kitap sayisi kadar donen bu dongude, her bir kitap icin yer acilip string kopyalama islemi yapiliyor. kitap sayisi degiskeni de guncelleniyor.
	while ((token = strtok(NULL, TOKEN_DELIMITERS)) != NULL)
	{
		(*book_count)++;
		book_names = (char**)realloc(book_names, (*book_count) * sizeof(char*));
		book_names[(*book_count) - 1] = (char*)calloc(strlen(token) + 1, sizeof(char));
		strcpy(book_names[(*book_count) - 1], token);
	}
	//son kitap icin olusan uc durum vardi. onu asmak icin boyle bir atama yapmak gerekiyor.
	book_names[(*book_count) - 1][strlen(book_names[(*book_count) - 2])] = '\0';
	free(token);
	return book_names;
}

//dosyada puan verilmeyen(okunmayan) kitaplari, 0 puan olarak yerlestiren fonksiyon. strtok fonksiyonu..
//null olan durumlarda calismadigi icin bu fonksiyonu yazdim.
void insertUnreadBookRatings(char* line)
{
	int i = 1; //dongu degiskeni
	int j = 0; //dongu degiskeni
	char new_line[BUFFER_SIZE]; //uzerinde string operasyonlari yapilacak gecici bi degisken
	for (i = 1; i < strlen(line); i++)
	{
		//eger mevcut karakter bi oncekiyle ayniysa
		if (line[i] == line[i - 1])
		{
			//eger bu karakter ayni zamanda csv seperator karakterine esitse, burada bir okunmamis kitap vardir
			if (line[i] == CSV_SEPERATOR)
			{
				//iki csv seperator arasina 0 puan oldugu bilgisi yerlestirilir.
				new_line[j] = CSV_SEPERATOR;
				j++;
				new_line[j] = '0';
				j++;
			}
			//yoksa devam ederiz.
			else
			{
				new_line[j] = line[i - 1];
				j++;
			}
		}
		//eger bosluk karakterine esitse, yine 0 puan yani okunmayan kitap vardir. 0 yerlestiririz.
		else if (line[i - 1] == ' ')
		{
			new_line[j] = '0';
			j++;
		}
		//yoksa devam ederiz.
		else
		{
			new_line[j] = line[i - 1];
			j++;
		}
	}
	//en son kitap icin uc durum senaryosu mevcuttu. onu asmak icin de sondan bi onceki karakter csv seperator ise son kitap okunmamistir.
	//0 yerlestiririz.
	if (line[i - 2] == CSV_SEPERATOR)
	{
		new_line[j] = '0';
		j++;
	}
	//en sona da str terminator karakterini koyariz
	new_line[j] = '\0';
	//sonra parametre olarak gelen line degiskenini guncelleriz.
	strcpy(line, new_line);
}

//dosyadan her bir kullanici ve kitaplara verdigi puanlari okudugum fonksiyon.
USER_DATA* populateUserArray(FILE* fp, int book_count, int* user_count, int* n_user_count)
{
	USER_DATA* users; //kullanicilar dizisi
	int eof_flag = 0; //dosya sonuna gelip gelmedigimi anlamak icin kullaniyorum
	int i; //dongu degiskeni
	int temp_user_count; //gecici olarak kullandigim, kullanici sayisini tutan degisken.
	char* ch; //fgetsin donusunu bu degiskende sakliyorum
	char line[BUFFER_SIZE]; //okunan satiri bu degiskende sakliyorum 
	char* token; //strtok fonksiyonu icin kullandim
	users = (USER_DATA*)calloc(1, sizeof(USER_DATA));
	*user_count = 0;

	//dosya sonuna gelene kadar dongude kal.
	while (eof_flag != 1)
	{
		ch = fgets(line, BUFFER_SIZE, fp);
		//null keldiyse dosya sonudur.
		if (ch == NULL)
		{
			eof_flag = 1;
		}
		//eger asagidaki karakterlerden biri geldiyse, U ve NU satirlari arasindaki bos satir kismindayiz demektir. 
		//U kullanicilarini ilgili degiskene aliriz.
		else if ((line[0] == ' ') || (line[0] == ';') || (line[0] == '0') || (line == NULL))
		{
			temp_user_count = *user_count;
		}
		else
		{
			//okunmayan kitaplar icin 0 puan yerlestirmesini yapariz.
			insertUnreadBookRatings(line);
			//kullanici adi alinir. kullanici adi bos degilse asagidaki islemler yapilir.
			if ((token = strtok(line, TOKEN_DELIMITERS)) != NULL)
			{
				//kullanici sayisi 1 artar.
				(*user_count)++;
				//ilgili yer acma islemleri yapilir.
				users = (USER_DATA*)realloc(users, (sizeof(USER_DATA) * (*user_count)));
				users[(*user_count) - 1].user_name = (char*)calloc(strlen(token), sizeof(char));
				strcpy(users[(*user_count) - 1].user_name, token);
				users[(*user_count) - 1].book_ratings = (int*)calloc(book_count, sizeof(int));
				//her bir kitap icin
				for (i = 0; i < book_count; i++)
				{
					//strtok ile verilen puan alinir.
					token = strtok(NULL, TOKEN_DELIMITERS);
					//csv dosyasinda okunmayan kitaplar icin bazen 0 bazen null bazen de bosluk karakteri geldigini gozlemledigim icin,
					//insertUnreadBookRatings fonksiyonuna ek olarak her ihtimale karsi bu kontrolu de yapiyorum. 
					if ((token == NULL) || (strcmp(token, "") == 0) || (strcmp(token, " ") == 0))
					{
						users[(*user_count) - 1].book_ratings[i] = 0;
					}
					else
					{
						//tokeni integera donusturup ilgili atama yapilir.
						users[(*user_count) - 1].book_ratings[i] = atoi(token);
					}
				}
			}
		}
	}
	//n_user_count, NU seklinde baslayan kullanicilari temsil ediyor.
	(*n_user_count) = (*user_count) - temp_user_count;
	(*user_count) = temp_user_count;
	free(ch);
	return users;
}

//kitap adlarini yazdiran bir fonksiyon.
void printBookNames(char** book_names, int book_count)
{
	int i;
	for (i = 0; i < book_count; i++)
	{
		printf("-%s=\n", book_names[i]);
	}
}

//kullanicilari yazdiran bir fonksiyon, kitaplara verdikleri oylarla birlikte.
void printUserArray(USER_DATA* users, int user_count, int book_count)
{
	int i;
	int j;
	for (i = 0; i < user_count; i++)
	{
		printf("%s\t", users[i].user_name);
		for (j = 0; j < book_count; j++)
		{
			printf("%d ", users[i].book_ratings[j]);
		}
		printf("\n");
	}
}

//iki kullanicinin benzerligini bulan fonksiyon.
double similarity(USER_DATA* user_1, USER_DATA* user_2, int book_count)
{
	int i; //dongu degiskeni
	int* common_read_book_indexes; //iki kullanicinin ortak okudugu kitaplarin, book_names matrisindeki satir indeksleri bu pointer dizisinde tutuluyor.
	int common_read_book_count = 0; //iki kullanicinin ortak okudugu kitap sayisi
	int user_1_read_book_count = 0; //1. kullanicinin okudugu kitap sayisi
	int user_2_read_book_count = 0; //2. kullanicinin okudugu kitap sayisi
	double sim = 0; //benzerlik degiskeni
	double sum = 0; //toplam icin kullanilan bir gecici degisken
	double sum2 = 0;//toplam icin kullanilan bir gecici degisken
	double mean_user_1 = 0; //1. kullanicinin puan ortalamasi
	double mean_user_2 = 0; //2. kullanicinin puan ortalamasi
	double current_sum_user_1 = 0; //toplam icin kullanilan bir degisken
	double current_sum_user_2 = 0; //toplam icin kullanilan bir degisken

	common_read_book_indexes = (int*)calloc(1, sizeof(int));
	//her bir kitap icin
	for (i = 0; i < book_count; i++)
	{
		//eger ikisi de 0 puan vermemisse ortak olarak okunmus bir kitaptir, ilgili atamalar yapilir.
		if ((user_1->book_ratings[i] != 0) && (user_2->book_ratings[i] != 0))
		{
			common_read_book_count++;
			common_read_book_indexes = (int*)realloc(common_read_book_indexes, sizeof(int) * common_read_book_count);
			common_read_book_indexes[common_read_book_count - 1] = i;
		}
	}
	//common_read_book_indexes = (int*)realloc(common_read_book_indexes, sizeof(int) * common_read_book_count);
	//eger ortak okunmus bir kitap yoksa fonksiyondan doneriz.
	if (common_read_book_count == 0)
	{
		printf("'%s' and '%s' has not have any common read books...Returning..\n", user_1->user_name, user_2->user_name);
		return -2;
	}

	//ortalama hesaplarken ortak kitap sayisina gore hesapliyordum fakat bazi durumlarda similarity= 0/0 yani (nan) ciktigini farkettigim icin degistirdim.
	//her kullanicinin kendi okudugu kitap sayisina gore buluyorum.

	/*
	//her iki kullanicinin verdikleri puanlar toplanir.
	for (i = 0; i < common_read_book_count; i++)
	{
		mean_user_1 += user_1->book_ratings[common_read_book_indexes[i]];
		mean_user_2 += user_2->book_ratings[common_read_book_indexes[i]];
	}
	//ortak okunan kitap sayisina bolunerek puan ortalamalari bulunur.
	mean_user_1 /= common_read_book_count;
	mean_user_2 /= common_read_book_count;
	*/
	//her iki kullanicinin verdikleri puanlar toplanir.
	//----------------------------------------------------------------------------------------------------------------
	for (i = 0; i < book_count; i++)
	{
		if (user_1->book_ratings[i] != 0)
		{
			mean_user_1 += user_1->book_ratings[i];
			user_1_read_book_count++;
		}
		if (user_2->book_ratings[i] != 0)
		{
			mean_user_2 += user_2->book_ratings[i];
			user_2_read_book_count++;
		}
	}
	//okunan kitap sayisina bolunerek puan ortalamalari bulunur.
	mean_user_1 /= user_1_read_book_count;
	mean_user_2 /= user_2_read_book_count;
	//----------------------------------------------------------------------------------------------------------------
	//bolme isleminin ust kismi hesaplanir.
	for (i = 0; i < common_read_book_count; i++)
	{
		sum = sum + (user_1->book_ratings[common_read_book_indexes[i]] - mean_user_1) * (user_2->book_ratings[common_read_book_indexes[i]] - mean_user_2);
	}

	//bolme isleminin alt kismi hesaplanir.
	for (i = 0; i < common_read_book_count; i++)
	{
		current_sum_user_1 += pow((user_1->book_ratings[common_read_book_indexes[i]] - mean_user_1), 2);
		current_sum_user_2 += pow((user_2->book_ratings[common_read_book_indexes[i]] - mean_user_2), 2);
	}
	//ilgili kok alma islemi yapilip sum2 hesaplanir.
	sum2 = sqrt(current_sum_user_1) * sqrt(current_sum_user_2);

	//son olarak bolme islemi yapilip benzerlik bulunur.
	sim = sum / sum2;
	free(common_read_book_indexes);
	return sim;
}

//kullanici adi verilen bir kullanicinin hangi indiste oldugu bilgisini donduren fonksiyon.
int getUserIndex(USER_DATA* users, int user_count, char* user_name)
{
	int i;
	for (i = 0; i < user_count; i++)
	{
		if (strcmp(user_name, users[i].user_name) == 0)
		{
			return i;
		}
	}
	printf("Could not find user with name '%s'...Returning..\n", user_name);
	return -1;
}

//similarity fonksiyonunun giris kismi, book suggestion haricinde sadece benzerlik bulunmak istendigi durumlar icin,
//fonksiyonu iki parcaya boldum. book suggestion kullanilirsa inputlar book suggestion kisminda alinir yoksa burada alinir.
void twoUserSimilarity(USER_DATA* users, int user_count, int n_user_count, int book_count)
{
	int user_1_index;
	int user_2_index;
	char user_1_name[MAX_WORD_SIZE];
	char user_2_name[MAX_WORD_SIZE];
	double sim;

	printf("Enter first user's name: ");
	scanf("%s", user_1_name);
	printf("Enter second user's name: ");
	scanf("%s", user_2_name);

	//girilen kullanici adlarina gore kullanici indisleri bulunur.
	user_1_index = getUserIndex(users, user_count + n_user_count, user_1_name);
	user_2_index = getUserIndex(users, user_count + n_user_count, user_2_name);

	if ((user_1_index == -1) || (user_2_index == -1))
	{
		return;
	}
	if (user_1_index == user_2_index)
	{
		printf("You entered same user name twice..Returning..\n");
		return;
	}
	sim = similarity(&users[user_1_index], &users[user_2_index], book_count);
	//eger sim = -2 ise, iki kullanicinin ortak okudugu kitap yoktur.
	if (sim == -2)
	{
		return;
	}
	printf("Similarity of '%s' and '%s': %f\n\n", user_1_name, user_2_name, sim);
}

//en benzer k kullaniciyi bulurken, ilk k kullanici hesaplandiktan sonra benzerlikler dizisini kucukten buyuge siraladim.
//boylece sonraki gelen kullanicilarda, benzerlik ilk kullanicidan kucukse hic bir islem yapilmadan devam edilecek, buyukse, ilgili indise yerlesmesi saglanip,
//o indisten oncekiler birer sola kaydirilacak ve ilk indisteki diziden silinmis olacak. Normal sort islemine gore daha efektif bir cozum.
void sortSimilaritiesArray(SIMILARITY_DATA* similarities, int k)
{
	int i; //dongu degiskeni
	int j; //dongu degiskeni
	int x; //indis icin kullanilan bir degisken
	SIMILARITY_DATA temp;
	//k kez donulur.
	for (i = 0; i < k; i++)
	{
		//
		x = -1;
		//gecici benzerlik veri yapisinin benzerlik degiskeni 2 olarak ilklendirilir. (normalde max 1 oldugu icin ondan buyuk bir deger sectim.)
		temp.similarity = 2;
		//mevcut indisten dizinin sonuna kadar gidilir ve siralama islemi yapilir.
		for (j = i; j < k; j++)
		{
			if (similarities[j].similarity < temp.similarity)
			{
				temp = similarities[j];
				x = j;
			}
		}
		if (x != -1)
		{
			similarities[x] = similarities[i];
			similarities[i] = temp;
		}
	}
}

//ciktida guzel gorunmesi icin kitap onerilerini tahmini puanlar buyukten kucuge olacak sekilde siralayan fonksiyon.
void sortBookSuggestionArray(BOOK_SUGGESTION_DATA* suggestions, int unread_book_count)
{
	int i; //dongu degiskeni
	int j; //dongu degiskeni
	int x; //indis erisimi icin kullanilan bir degisken
	BOOK_SUGGESTION_DATA temp;

	//okunmayan kitaplarin siralama islemi yapilir.
	for (i = 0; i < unread_book_count; i++)
	{
		x = -1;
		temp.predicted_rating = -1;
		for (j = i; j < unread_book_count; j++)
		{
			if (suggestions[j].predicted_rating > temp.predicted_rating)
			{
				temp = suggestions[j];
				x = j;
			}
		}
		if (x != -1)
		{
			suggestions[x] = suggestions[i];
			suggestions[i] = temp;
		}
	}
}

//en benzer k kullanicinin bulundugu fonksiyon.
SIMILARITY_DATA* kMostSimilars(USER_DATA* users, int user_count, int n_user_count, int book_count, char* user_name, int user_index, int k)
{
	int i; //dongu degiskeni
	int j; //dongu degiskeni
	int x; //dongu degiskeni
	int same_index_flag = 0; //kullanicinin kendisiyle benzerliginin bulunmasinin onune gecmek icin kullandigim flag
	SIMILARITY_DATA* similarities; //benzerlikler dizisi.
	double sim; //benzerlik degiskeni

	//benzerlikler dizisine k kadar yer aciyorum
	similarities = (SIMILARITY_DATA*)calloc(k, sizeof(SIMILARITY_DATA));
	//her bir kullanici icin
	for (i = 0; i < k; i++)
	{
		//eger k benzer kullanici bulunacak olan kullanici, ilk k elemanin icindeyse kendisiyle benzerligi bulunmasin diye kontrol ediyorum.
		if (i != user_index)
		{
			//kullanici indisi atiyorum ve benzerligi similarity fonksiyonuyla bulup onu da atiyorum.
			similarities[i].user_index = i;
			similarities[i].similarity = similarity(&users[user_index], &users[i], book_count);
		}
		else
		{
			//benzerligi bulunmak istenenle mevcut indis ayniysa flagi set ediyorum.
			same_index_flag = 1;
		}

	}
	//eger for dongusunden ciktigimda same_index_flag set edilmisse, ilgili yere donguden ciktigim andaki i ve similarityi atiyorum, yani k+1. kullanicininki oluyor.
	if (same_index_flag)
	{
		similarities[user_index].user_index = i;
		similarities[user_index].similarity = similarity(&users[user_index], &users[i], book_count);
	}

	//kullanicilar dizisindeki ilk k kullaniciya baktiktan sonra k uzunlugundaki benzerlikler dizisini kucukten buyuge siraliyorum.
	sortSimilaritiesArray(similarities, k);


	//bundan sonra ise, eger gelen kullanicinin benzerligi, benzerliklerdizisi[0] dan kucukse, zaten o diziye giremez diyip geciyorum.
	//buyukse de kucuk oldugu kisma gelene kadar dizi uzerinde ilerleyip ilgili gozu bulunca insert ediyorum. benzerliklerdizisi[0] in yerinde [1], [1] in yerine [2] gelecek sekilde,,
	//kullanicinin yerlestigi gozden geriye dogru bir adim kaydirma islemi yapiliyor. gelen kullanicinin direkt ilgili goze yerlesmesi, butun kullanicilari siralamaktan daha efektif bir yontem.

	//eger same_index_flag set ise, k+1 yoksa k'dan baslayip kullanici sayisina kadar dongude kalinir.
	for (i = (same_index_flag ? k + 1 : k); i < user_count; i++)
	{
		//eger i, kullanici indisine esitse islem yapmayiz.
		if (i == user_index)
		{
			continue;
		}
		j = 0;
		//kullanici ve i. kullanici arasindaki benzerlik hesaplanir.
		sim = similarity(&users[user_index], &users[i], book_count);
		//hesaplanan benzerlige gore i. kullanicinin hangi indise yerlesecegi hesaplanir.
		while ((j < k) && (sim > similarities[j].similarity))
		{
			j++;
		}
		//j artmamissa i. kullanicinin benzerligi, similarities dizisindeki butun benzerliklerden kucuktur, devam ederiz.
		if (j == 0)
		{
			continue;
		}
		//yoksa ilgili indisten oncesini birer asagi kaydirir ve 0. indistekini yok ederiz.
		for (x = 0; x < j - 1; x++)
		{
			similarities[x].similarity = similarities[x + 1].similarity;
			similarities[x].user_index = similarities[x + 1].user_index;
		}
		//sonra i. kisinin benzerligi ve indisi atanir diziye.
		similarities[j - 1].similarity = sim;
		similarities[j - 1].user_index = i;
	}

	//yazdirma islemleri.
	printf("The most similar %d users to '%s'\n", k, user_name);
	for (i = k - 1; i > -1; i--)
	{
		printf("%3d| %-3s | %10f\n", k - i, users[similarities[i].user_index].user_name, similarities[i].similarity);
	}

	return similarities;
}

//en benzer k kisiyi buldugumuz fonksiyonun girisi. book suggestion cagrilirsa input alinma islemi orada, yoksa sadece k benzer bulunmak istenirse input burada alinabilmesi icin,
//iki parcaya boldum fonksiyonu.
SIMILARITY_DATA* kMostSimilarsEntry(USER_DATA* users, int user_count, int n_user_count, int book_count)
{
	char user_name[MAX_WORD_SIZE];
	int user_index;
	int k;
	printf("Enter user name: ");
	scanf("%s", user_name);
	user_index = getUserIndex(users, user_count + n_user_count, user_name);
	if (user_index == -1)
	{
		return NULL;
	}
	printf("Enter k: ");
	scanf("%d", &k);
	//k negatif girilmesi durumunu kontrol ederiz.
	if (k < 1)
	{
		printf("k must be positive..Returning..\n");
		return NULL;
	}
	//k, U ile baslayan kullanici sayisindan fazlaysa diye kontrol ediyorum.
	if (k > user_count)
	{
		printf("You entered '%d', but there are only '%d' users..\n", k, user_count);
		k = user_count;
	}
	//eger, oneri yapilacak kullanici NU degil de U ile basliyorsa, indisi k'dan yani U ile baslayan kullanici sayisindan kucuktur. o durumda tasma olmamasi yani NU'larin isleme dahil edilmemesi yani
	//uc deger istisnasi olusmamasi icin k'yi bir azaltiyorum.
	if (k == user_count && user_index < k)
	{
		k--;
	}
	return kMostSimilars(users, user_count, n_user_count, book_count, user_name, user_index, k);
}

//Kullaniciya kitap onerisi yapildiktan sonra bu oneri ve en benzer oldugu k kullanici dosyaya yazilir.
void saveToFile(USER_DATA* users, SIMILARITY_DATA* similarities, char* user_name, char* most_suggested_book, int k)
{
	FILE* fp;
	char* file_name;
	int i = 0;
	char number[16];
	//_itoa(k, number, 10);
	snprintf(number, sizeof(number), "%d", k);
	file_name = (char*)calloc(strlen(user_name) + 1 + strlen("_kitap_onerisi_ve_en_benzer_") + strlen("_kullanici.csv") + strlen(number) + strlen(BOOK_SUGGESTIONS_DIRECTORY) + strlen(".//"), sizeof(char));
	file_name = strcpy(file_name, "./");
	file_name = strcat(file_name, BOOK_SUGGESTIONS_DIRECTORY);
	file_name = strcat(file_name, "/");
	file_name = strcat(file_name, user_name);
	file_name = strcat(file_name, "_kitap_onerisi_ve_en_benzer_");
	file_name = strcat(file_name, number);
	file_name = strcat(file_name, "_kullanici.csv");
	fp = fopen(file_name, "w");
	if (fp == NULL)
	{
		printf("Can't open output file for writing..\nMake sure the following file is not open in your computer right now..Returning..\n");
		printf("'%s'\n", file_name);
		return;
	}
	fprintf(fp, "User name%c%s\n", CSV_SEPERATOR, user_name);

	fprintf(fp, "%d most similar users%c", k, CSV_SEPERATOR);
	for (i = k - 1; i > -1; i--)
	{
		fprintf(fp, "%s%c", users[similarities[i].user_index].user_name, CSV_SEPERATOR);
	}
	fprintf(fp, "\n");
	fprintf(fp, "Suggested book%c%s\n", CSV_SEPERATOR, most_suggested_book);
	fclose(fp);
	free(file_name);
}

//kitap onerisi yapan fonksiyon
void suggestBook(USER_DATA* users, char** book_names, int user_count, int n_user_count, int book_count)
{
	int i; //dongu degiskeni
	int j; //dongu degiskeni
	int k; //en benzer olarak bulunacak k kisi degiskeni
	int x; //dongu degiskeni
	int* unread_book_indexes; //oneri yapilacak kisinin okumadigi kitaplarin indisleri bu dizide tutulur
	int unread_book_count = 0; //oneri yapilacak kisinin okumadigi kitap sayisi
	int user_index = 0; //oneri yapilacak kisinin indisi, scanf ile bu degiskende tutulur
	int other_user_read_book_count = 0; //k adet kisinin her biri icin hesaplanacak olan, o kisinin okudugu kitap sayisini tutan degisken
	double mean = 0; //oneri yapilacak kisinin verdigi puanlarin ortalamasi
	double other_users_mean = 0; //diger kullanicinin puan ortalamasi
	BOOK_SUGGESTION_DATA* preds; //kitaplara verilecek tahmini puanlarin tutuldugu dizi.
	double current_pred = 0; //dongu icinde o anki kitaba verilecek tahmini puan bu degiskende tutulur.
	double sum1 = 0; //paydaki toplama isleminin sonucu tutulur
	double sum2 = 0; //paydadaki toplama isleminin sonucu tutulur
	char user_name[MAX_WORD_SIZE]; //kullanici adi tutulur
	SIMILARITY_DATA* similarities; //benzerlikler dizisi.

	printf("Enter user name to suggest a book: ");
	scanf("%s", user_name);

	//girilen kullanici adina gore kullanici indisi bulunur. bulunamazsa fonksiyondan donulur.
	user_index = getUserIndex(users, user_count + n_user_count, user_name);
	if (user_index == -1)
	{
		return;
	}

	printf("Enter k: ");
	scanf("%d", &k);
	//k negatif girilmesi durumunu kontrol ederiz.
	if (k < 1)
	{
		printf("k must be positive..Returning..\n");
		return;
	}

	//k, U ile baslayan kullanici sayisindan fazlaysa diye kontrol ediyorum.
	if (k > user_count)
	{
		printf("You entered '%d', but there are only '%d' users..\n", k, user_count);
		k = user_count;
	}

	//for icinde, oneri yapilacak kisinin okumadigi kitap sayisi ve indisleri ilgili yerlere atanir.
	unread_book_indexes = (int*)calloc(1, sizeof(int));
	for (i = 0; i < book_count; i++)
	{
		if (users[user_index].book_ratings[i] == 0)
		{
			unread_book_count++;
			unread_book_indexes = (int*)realloc(unread_book_indexes, sizeof(int) * unread_book_count);
			unread_book_indexes[unread_book_count - 1] = i;
		}
	}
	//unread_book_indexes = (int*)realloc(unread_book_indexes, sizeof(int) * unread_book_count);

	//okumadigi kitap yoksa fonksiyondan donulur.
	if (unread_book_count == 0)
	{
		printf("'%s' has read all books...Returning..\n", user_name);
		return;
	}

	//tahmini puan dizisine okumadigi kitap sayisi kadar elemanlik yer acilir.
	preds = (BOOK_SUGGESTION_DATA*)calloc(unread_book_count, sizeof(BOOK_SUGGESTION_DATA));

	//eger, oneri yapilacak kullanici NU degil de U ile basliyorsa, indisi k'dan yani U ile baslayan kullanici sayisindan kucuktur. o durumda tasma olmamasi yani NU'larin isleme dahil edilmemesi yani
	//uc deger istisnasi olusmamasi icin k'yi bir azaltiyorum.
	if (k == user_count && user_index < k)
	{
		k--;
	}
	//benzerlikler dizisi kmostsimilars fonksiyonu ile bulunur
	similarities = kMostSimilars(users, user_count, n_user_count, book_count, user_name, user_index, k);

	//kullanicinin verdigi puanlarin ortalamasi hesaplanir.
	for (x = 0; x < book_count; x++)
	{
		mean += users[user_index].book_ratings[x];
	}
	mean /= ((double)book_count - unread_book_count);

	//oneri yapilacak kullanicinin okumadigi her bir kitap icin bu dongu calisir.
	for (i = 0; i < unread_book_count; i++)
	{
		sum1 = 0; //formuldeki pay kismi hesaplanirken kullanilir.
		sum2 = 0; //formuldeki payda kismi hesaplanirken kullanilir.
		//k sayisi kadar yani benzerlik hesaplanan kisi sayisi kadar bu dongu calisir.
		for (j = 0; j < k; j++)
		{
			other_users_mean = 0;
			other_user_read_book_count = 0;
			//diger kullanicinin puan ortalamasi hesaplanir ve okudugu kitap sayisi hesaplanir.
			for (x = 0; x < book_count; x++)
			{
				other_users_mean += users[similarities[j].user_index].book_ratings[x];
				if (users[similarities[j].user_index].book_ratings[x] != 0)
				{
					other_user_read_book_count++;
				}
			}
			//diger kullanicinin puan ortalamasi hesaplanir.
			other_users_mean /= other_user_read_book_count;

			//formuldeki pay kismi hesaplanir
			sum1 += (similarities[j].similarity * (users[similarities[j].user_index].book_ratings[unread_book_indexes[i]] - other_users_mean));
			//formuldeki payda kismi hesaplanir
			sum2 += (similarities[j].similarity);
		}
		//mevcut kitaba verilecek tahmini puan hesaplanir
		current_pred = mean + sum1 / sum2;
		//ilgili atamalar yapilir.
		preds[i].book_index = unread_book_indexes[i];
		preds[i].predicted_rating = current_pred;
	}

	//oneriler dizisi siralanir. ciktida duzgun gozukmesi icin.
	sortBookSuggestionArray(preds, unread_book_count);

	//yazdirma islemleri.
	printf("Book suggestions and predicted ratings for '%s'\n", user_name);
	for (i = 0; i < unread_book_count; i++)
	{
		printf("%-2d| %-20s| %10f\n", i + 1, book_names[preds[i].book_index], preds[i].predicted_rating);
	}
	printf("The book to be suggested is: '");

	//onerilen kitabin rengi farkli yapilir. guzel gozukmesi icin.
#ifdef _WIN32
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN | FOREGROUND_INTENSITY);
	printf("%s", book_names[preds[0].book_index]);
#elif _WIN64
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN | FOREGROUND_INTENSITY);
	printf("%s", book_names[preds[0].book_index]);
#else
	printf("\033[0;32m%s", book_names[preds[0].book_index]);
	printf("\033[0;37m");
#endif
#ifdef _WIN32
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
#elif _WIN64
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
#else
#endif

	printf("'\n");

	free(unread_book_indexes);
	saveToFile(users, similarities, user_name, book_names[preds[0].book_index], k);
}

int main()
{
	FILE* fp; //okunacak dosya pointeri
	USER_DATA* users; //kullanicilar dizisi
	char** book_names; //kitap isimleri matrisi
	int book_count = 0; //kitap sayisi hesaplanir
	int user_count = 0; //kullanici sayisi hesaplanir
	int n_user_count = 0; //NU ile baslayan kullanici sayisi hesaplanir.
	int opCode = 0; //yapilacak islemin kodu

	setlocale(LC_ALL, "Turkish");
	//konsol rengi ayarlanir
#ifdef _WIN32
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
#elif _WIN64
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
#else
	printf("\033[0;37m");
#endif	

	//dosya acma islemi yapilir
	fp = fopen(RECOMMENDATION_FILE_NAME, "r");
	if (fp == NULL)
	{
		//dosya bulunamazsa hata verilip program kapatilir.
		printf("Can't open the file '%s'.. Exiting..", RECOMMENDATION_FILE_NAME);
		exit(-1);
	}

#ifdef _WIN32
	_mkdir(BOOK_SUGGESTIONS_DIRECTORY, 0777);
#elif _WIN64
	_mkdir(BOOK_SUGGESTIONS_DIRECTORY, 0777);
#elif __linux__
	mkdir(BOOK_SUGGESTIONS_DIRECTORY, 0777);
#endif

	//kitap isimleri okunur
	book_names = populateBookNamesArray(fp, &book_count);
	//printBookNames(book_names, book_count);

	//kullanicilar dizisi olusturulur.
	users = populateUserArray(fp, book_count, &user_count, &n_user_count);
	//printUserArray(users, user_count, book_count);

	do
	{
		printf("0-Exit\n1-Calculate two users' similarity\n2-Calculate k most similar users to a user\n3-Suggest new book to user\n\nEnter the operation code: ");
		//printf("0-Exit\n1-Iki kullanicinin benzerligini hesapla\n2-Bir kullaniciya en benzer k adet kullanici hesapla\n3-Kitap oner\n\nIslem kodunu giriniz: ");
		scanf("%d", &opCode);

		switch (opCode)
		{
		case 0:
			break;
		case 1:
			//1 girildiysa iki kullanici benzerligi hesaplanir
			twoUserSimilarity(users, user_count, n_user_count, book_count);
			break;
		case 2:
			//2 girildiyse en benzer k kullanici hesaplanir
			kMostSimilarsEntry(users, user_count, n_user_count, book_count);
			break;
		case 3:
			//3 girildiyse kitap onerisi yapilir.
			suggestBook(users, book_names, user_count, n_user_count, book_count);
			break;
		default:
			printf("Hatali giris yaptiniz..!!\n");
			break;
		}
		printf("\n\n");
	} while (opCode);

	fclose(fp);
	return 0;
}
