# Interactive Watchlist with Azure Search
Let us image that in a car auction house which has a weekly list listing al newly available cars for auction. This auction house has a number of registered buyers. Each buyer has a watchlist for the cars they are interested in which is entered by the buyer. Also a buyer’s watchlist should be updated based on what he searched.

Based on Microsoft Azure Search team’s sample code, we build up a POC with Azure Search, Azure SQL DB and Azure App Service as described in the PDF file. 

There are two C# apps in this POC and both run in the Azure App Service. The first on is a web job, called DataIndexer. After the weekly list is loaded into the DB, the Azure Search Indexer will be updated. After the update, the buyers’ watchlist will be refreshed accordingly. The second one is a MVC app, called SimpleSearchMVCApp. It takes in the buyer’s name and then displays his watchlist. It also allows him to search the list and every tiem he picked up a car in the list his watchlist will be updated (upsert). 
