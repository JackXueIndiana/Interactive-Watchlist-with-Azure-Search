using System;
using System.Configuration;
using System.Threading;
using Microsoft.Azure.Search;
using Microsoft.Azure.Search.Models;
using System.Data.SqlClient;

namespace DataIndexer
{
    class Program
    {
        private const string AucionSearchIndex = "auctionsearch";
        private const string AuctionSearchDataSource = "auctionsearch-datasource";
        private const string AunctionSearchIndexer = "auctionsearch-indexer";

        private static ISearchServiceClient _searchClient;
        private static ISearchIndexClient _indexClient;

        // This Sample shows how to delete, create, upload documents and query an index
        static void Main(string[] args)
        {
            string searchServiceName = ConfigurationManager.AppSettings["SearchServiceName"];
            string apiKey = ConfigurationManager.AppSettings["SearchServiceApiKey"];

            // Create an HTTP reference to the catalog index
            try
            {
                _searchClient = new SearchServiceClient(searchServiceName, new SearchCredentials(apiKey));
                _indexClient = _searchClient.Indexes.GetClient(AucionSearchIndex);

                Console.WriteLine("{0}", "Deleting index, data source, and indexer...\n");
                if (DeleteIndexingResources())
                {
                    Console.WriteLine("{0}", "Creating index...\n");
                    CreateIndex();
                    Console.WriteLine("{0}", "Sync documents from Azure SQL...\n");
                    SyncDataFromAzureSQL();
                    GenerateSuggestListBasedOnWatchListInAzureSQL();
                }
                Console.WriteLine("{0}", "Complete.  Press any key to end application...\n");

                //Console.ReadKey();
            }
            catch (Exception ex)
            {
                Console.WriteLine("Error : {0}\r\n", ex.Message);
            }
        }

        private static bool DeleteIndexingResources()
        {
            // Delete the index, data source, and indexer.
            try
            {
                _searchClient.Indexes.Delete(AucionSearchIndex);
                _searchClient.DataSources.Delete(AuctionSearchDataSource);
                _searchClient.Indexers.Delete(AunctionSearchIndexer);
            }
            catch (Exception ex)
            {
                Console.WriteLine("Error deleting indexing resources: {0}\r\n", ex.Message);
                Console.WriteLine("Did you remember to add your SearchServiceName and SearchServiceApiKey to the app.config?\r\n");
                return false;
            }

            return true;
        }

        private static void CreateIndex()
        {
            // Create the Azure Search index based on the included schema
            try
            {
                var definition = new Index()
                {
                    Name = AucionSearchIndex,
                    Fields = new[] 
                    { 
                        new Field("AuctionUniqueID",     DataType.String) { IsKey = true, IsSearchable = true,  IsFilterable = true,  IsSortable = true,  IsFacetable = false, IsRetrievable = true},        //{ IsKey = true,  IsSearchable = false, IsFilterable = false, IsSortable = true, IsFacetable = false, IsRetrievable = true},
                        new Field("VIN",   DataType.String)         { IsKey = false, IsSearchable = true,  IsFilterable = true,  IsSortable = true,  IsFacetable = false, IsRetrievable = true},
                        new Field("ModelYear",   DataType.String)         { IsKey = false, IsSearchable = true,  IsFilterable = true,  IsSortable = true,  IsFacetable = false, IsRetrievable = true},
                        new Field("Make",   DataType.String)         { IsKey = false, IsSearchable = true,  IsFilterable = true,  IsSortable = true,  IsFacetable = false, IsRetrievable = true},
                        new Field("Model",   DataType.String)         { IsKey = false, IsSearchable = true,  IsFilterable = true,  IsSortable = true,  IsFacetable = false, IsRetrievable = true},
                        new Field("Series",   DataType.String)         { IsKey = false, IsSearchable = true,  IsFilterable = true,  IsSortable = true,  IsFacetable = false, IsRetrievable = true},
                        new Field("BodyStyle",   DataType.String)         { IsKey = false, IsSearchable = true,  IsFilterable = true,  IsSortable = true,  IsFacetable = false, IsRetrievable = true},
                        new Field("Color",   DataType.String)         { IsKey = false, IsSearchable = true,  IsFilterable = true,  IsSortable = true,  IsFacetable = false, IsRetrievable = true},
                        new Field("InteriorColor",   DataType.String)         { IsKey = false, IsSearchable = true,  IsFilterable = true,  IsSortable = true,  IsFacetable = false, IsRetrievable = true},
                        new Field("Mileage",   DataType.String)         { IsKey = false, IsSearchable = true,  IsFilterable = true,  IsSortable = true,  IsFacetable = false, IsRetrievable = true}
                }
                };

                _searchClient.Indexes.Create(definition);
            }
            catch (Exception ex)
            {
                Console.WriteLine("Error creating index: {0}\r\n", ex.Message);
            }

        }

        private static void SyncDataFromAzureSQL()
        {
            // This will use the Azure Search Indexer to synchronize data from Azure SQL to Azure Search
            Console.WriteLine("{0}", "Creating Data Source...\n");
            var dataSource =
                DataSource.AzureSql(
                    name: AuctionSearchDataSource,
                    sqlConnectionString: ConfigurationManager.AppSettings["SqlConnectionString"],
                    tableOrViewName: "[dbo].[Auction]",
                    description: "Auction table and weekly updated");

            try
            {
                _searchClient.DataSources.Create(dataSource);
            }
            catch (Exception ex)
            {
                Console.WriteLine("Error creating data source: {0}", ex.Message);
                return;
            }

            Console.WriteLine("{0}", "Creating Indexer and syncing data...\n");

            var indexer =
                new Indexer()
                {
                    Name = AunctionSearchIndexer,
                    Description = "Auction data indexer",
                    DataSourceName = dataSource.Name,
                    TargetIndexName = AucionSearchIndex
                };

            try
            {
                _searchClient.Indexers.Create(indexer);
            }
            catch (Exception ex)
            {
                Console.WriteLine("Error creating and running indexer: {0}", ex.Message);
                return;
            }

            bool running = true;
            Console.WriteLine("{0}", "Synchronization running...\n");
            while (running)
            {
                IndexerExecutionInfo status = null;

                try
                {
                    status = _searchClient.Indexers.GetStatus(indexer.Name);
                }
                catch (Exception ex)
                {
                    Console.WriteLine("Error polling for indexer status: {0}", ex.Message);
                    return;
                }

                IndexerExecutionResult lastResult = status.LastResult;
                if (lastResult != null)
                {
                    switch (lastResult.Status)
                    {
                        case IndexerExecutionStatus.InProgress:
                            Console.WriteLine("{0}", "Synchronization running...\n");
                            Thread.Sleep(1000);
                            break;

                        case IndexerExecutionStatus.Success:
                            running = false;
                            Console.WriteLine("Synchronized {0} rows...\n", lastResult.ItemCount);
                            break;

                        default:
                            running = false;
                            Console.WriteLine("Synchronization failed: {0}\n", lastResult.ErrorMessage);
                            break;
                    }
                }
            }
        }

        static string Tsql_SelectWatchedItems()
        {
            return @"
            SELECT
                WatchItemId,
	            VIN,
	            ModelYear,
	            Make,
	            Model,
	            Series,
	            BodyStyle,
	            Color,
                InternalColor,
	            Mileage,
                SearchString,
	            BuyerName
            FROM WatchItem;";
        }
        private static void GenerateSuggestListBasedOnWatchListInAzureSQL()
        {
            Console.WriteLine("{0}", "Creating Data Source for table Watch...\n");
            SuggestSearch ss = new SuggestSearch();

            try
            {
                using (var connection = new SqlConnection(ConfigurationManager.AppSettings["SqlConnectionString"]))
                {
                    connection.Open();

                    using (var command = new SqlCommand(Tsql_SelectWatchedItems(), connection))
                    {
                        using (SqlDataReader reader = command.ExecuteReader())
                        {
                            while (reader.Read())
                            {
                                Int32 watchItemId = reader.GetInt32(0);
                                String VIN = reader.IsDBNull(1) ? null : reader.GetString(1);
                                Decimal? modelYear = reader.IsDBNull(2) ? (Decimal?)null : reader.GetDecimal(2);
                                String make = reader.IsDBNull(3) ? null : reader.GetString(3);
                                String model = reader.IsDBNull(4) ? null : reader.GetString(4);
                                String series = reader.IsDBNull(5) ? null : reader.GetString(5);
                                String bodyStype = reader.IsDBNull(6) ? null : reader.GetString(6);
                                String color = reader.IsDBNull(7) ? null : reader.GetString(7);
                                String internalColor = reader.IsDBNull(8) ? null : reader.GetString(8);
                                Decimal? mileage = reader.IsDBNull(9) ? (Decimal?)null : reader.GetDecimal(9);
                                String searchString = reader.IsDBNull(10) ? null : reader.GetString(10);
                                String buyerName = reader.IsDBNull(11) ? null : reader.GetString(11);

                                String searchText = String.IsNullOrWhiteSpace(VIN) ? VIN + " " : "";
                                searchText += modelYear.HasValue ? Convert.ToInt32(modelYear) + " " : "";
                                searchText += String.IsNullOrWhiteSpace(make) ? "" : make + " ";
                                searchText += String.IsNullOrWhiteSpace(model) ? "" : model + " ";
                                searchText += String.IsNullOrWhiteSpace(series) ? "" : series + " ";
                                searchText += String.IsNullOrWhiteSpace(color) ? "" : color + " ";
                                searchText += String.IsNullOrWhiteSpace(internalColor) ? "" : internalColor + " ";
                                searchText += mileage.HasValue ? Convert.ToInt32(mileage) + " " : "";
                                searchText += String.IsNullOrWhiteSpace(searchString) ? "" : searchString + " ";
                                searchText = searchText.Trim();

                                Console.WriteLine("searchText :");
                                Console.WriteLine("{0}", searchText);

                                DocumentSearchResult dsr = ss.SuggestSearchImpl(searchText);

                                if (dsr != null && dsr.Results.Count > 0)
                                {
                                    foreach (SearchResult sr in dsr.Results)
                                    {
                                        Document d = sr.Document;
                                        
                                        Console.WriteLine("{0}", d["AuctionUniqueID"]);
                                        Console.WriteLine("{0}", d["VIN"]);
                                        Console.WriteLine("{0}", d["ModelYear"]);
                                        Console.WriteLine("{0}", d["Make"]);
                                        Console.WriteLine("{0}", d["Model"]);
                                        Console.WriteLine("{0}", d["Series"]);
                                        Console.WriteLine("{0}", d["BodyStyle"]);
                                        Console.WriteLine("{0}", d["Color"]);
                                        Console.WriteLine("{0}", d["InteriorColor"]);
                                        Console.WriteLine("{0}", d["Mileage"]);
                                        Console.WriteLine("{0}", buyerName);

                                        var connection1 = new SqlConnection(ConfigurationManager.AppSettings["SqlConnectionString"]);
                                        connection1.Open();

                                        SqlCommand Cmd = new SqlCommand("If Not Exists(select * from [dbo].[SuggestItem] where SuggestItemId = @SuggestItemId AND BuyerName = @BuyerName) " +
                                        "Begin INSERT INTO [dbo].[SuggestItem] " +
                                        "(SuggestItemId, VIN, ModelYear, Make, Model, Series, BodyStyle, Color, Mileage, BuyerName) " +
                                        "VALUES(@SuggestItemId, @VIN, @ModelYear, @Make, @Model, @Series, @BodyStyle, @Color, @Mileage, @BuyerName); End", connection1);

                                        Cmd.Parameters.Add("@SuggestItemId", System.Data.SqlDbType.NVarChar);
                                        Cmd.Parameters.Add("@VIN", System.Data.SqlDbType.NVarChar);
                                        Cmd.Parameters.Add("@ModelYear", System.Data.SqlDbType.Decimal);
                                        Cmd.Parameters.Add("@Make", System.Data.SqlDbType.NVarChar);
                                        Cmd.Parameters.Add("@Model", System.Data.SqlDbType.NVarChar);
                                        Cmd.Parameters.Add("@Series", System.Data.SqlDbType.NVarChar);
                                        Cmd.Parameters.Add("@BodyStyle", System.Data.SqlDbType.NVarChar);
                                        Cmd.Parameters.Add("@Color", System.Data.SqlDbType.NVarChar);
                                        Cmd.Parameters.Add("@Mileage", System.Data.SqlDbType.Decimal);
                                        Cmd.Parameters.Add("@BuyerName", System.Data.SqlDbType.NVarChar);

                                
                                        Cmd.Parameters["@SuggestItemId"].Value = d["AuctionUniqueID"];
                                        Cmd.Parameters["@VIN"].Value = d["VIN"];
                                        Cmd.Parameters["@ModelYear"].Value = d["ModelYear"];
                                        Cmd.Parameters["@Make"].Value = d["Make"];
                                        Cmd.Parameters["@Model"].Value = d["Model"];
                                        Cmd.Parameters["@Series"].Value = d["Series"];
                                        Cmd.Parameters["@BodyStyle"].Value = d["BodyStyle"];
                                        Cmd.Parameters["@Color"].Value = d["Color"];
                                        Cmd.Parameters["@Mileage"].Value = d["Mileage"];
                                        Cmd.Parameters["@BuyerName"].Value = buyerName;

                                        int RowsAffected = Cmd.ExecuteNonQuery();
                                        Console.Write("RowsAffected: ");
                                        Console.WriteLine("{0}", RowsAffected);
                                        connection1.Close();
                                    }
                                }

                            }
                        }
                    }
                    connection.Close();
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine("Error in getting data from table WatchItem: {0}", ex.Message);
                return;
            } 
        }
    }

    public class SuggestSearch
    {
        private static ISearchServiceClient _searchClient;
        private static ISearchIndexClient _indexClient;

        public static string errorMessage;

        static SuggestSearch()
        {
            try
            {
                string searchServiceName = ConfigurationManager.AppSettings["SearchServiceName"];
                string apiKey = ConfigurationManager.AppSettings["SearchServiceApiKey"];

                // Create an HTTP reference to the catalog index
                _searchClient = new SearchServiceClient(searchServiceName, new SearchCredentials(apiKey));
                _indexClient = _searchClient.Indexes.GetClient("auctionsearch");
            }
            catch (Exception e)
            {
                errorMessage = e.Message.ToString();
            }
        }

        public DocumentSearchResult SuggestSearchImpl(string searchText)
        {
            // Execute search based on query string
            try
            {
                SearchParameters sp = new SearchParameters()
                {
                    SearchFields = new[] {
                    "VIN",
                    "ModelYear",
                    "Make",
                    "Model",
                    "Series",
                    "BodyStyle",
                    "Color",
                    "InteriorColor",
                    "Mileage"},
                    SearchMode = SearchMode.Any,
                    Top = 1
                };
                return _indexClient.Documents.Search(searchText, sp);
            }
            catch (Exception ex)
            {
                Console.WriteLine("Error querying index: {0}\r\n", ex.Message.ToString());
            }
            return null;
        }

    }
}
